#include "infrastructure/search/LocalSearchAssistantRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QThread>
#include <QUrl>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
constexpr int kStartupTimeoutMs = 30000;

QString environmentPath(const char *name)
{
    return QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv(name)).trimmed());
}

quint16 reserveLoopbackPort()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    const auto port = server.serverPort();
    server.close();
    return port;
}

QString detectGpuDevice(const QString &executablePath, QString *errorMessage)
{
    QProcess probe;
#ifdef Q_OS_WIN
    probe.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *arguments) {
            arguments->flags |= CREATE_NO_WINDOW;
        });
#endif
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(QDir::toNativeSeparators(executablePath),
                {QStringLiteral("--list-devices")});
    if (!probe.waitForStarted(3000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法探测 GPU：%1").arg(probe.errorString());
        }
        return {};
    }
    if (!probe.waitForFinished(5000)) {
        probe.kill();
        probe.waitForFinished(1000);
        if (errorMessage) {
            *errorMessage = QStringLiteral("GPU 探测超时");
        }
        return {};
    }

    const auto output = QString::fromLocal8Bit(probe.readAll());
    bool readingDevices = false;
    const auto lines = output.split(QLatin1Char('\n'));
    for (const auto &rawLine : lines) {
        const auto line = rawLine.simplified();
        if (line.compare(QStringLiteral("Available devices:"), Qt::CaseInsensitive) == 0) {
            readingDevices = true;
            continue;
        }
        if (!readingDevices || line.isEmpty()) {
            continue;
        }
        const auto separator = line.indexOf(QLatin1Char(':'));
        return separator >= 0 ? line.mid(separator + 1).trimmed() : line;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("未检测到可用 GPU；内置文本模型已停用，不会回落到 CPU");
    }
    return {};
}
}

LocalSearchAssistantRuntime::LocalSearchAssistantRuntime(QString executablePath,
                                                         QString modelPath,
                                                         QObject *parent)
    : QObject(parent)
    , m_executablePath(executablePath.trimmed().isEmpty()
                           ? defaultExecutablePath()
                           : QDir::fromNativeSeparators(executablePath.trimmed()))
    , m_modelPath(modelPath.trimmed().isEmpty()
                      ? defaultModelPath()
                      : QDir::fromNativeSeparators(modelPath.trimmed()))
    , m_process(new QProcess(this))
    , m_network(new QNetworkAccessManager(this))
{
    m_probeTimer.setInterval(250);
    m_network->setProxy(QNetworkProxy::NoProxy);
#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *arguments) {
            arguments->flags |= CREATE_NO_WINDOW;
        });
#endif
    connect(&m_probeTimer, &QTimer::timeout,
            this, &LocalSearchAssistantRuntime::probeHealth);
    connect(m_process, &QProcess::started, this, [this]() {
        m_startElapsed.restart();
        m_probeTimer.start();
        probeHealth();
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        appendDiagnostic(m_process->readAllStandardError());
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendDiagnostic(m_process->readAllStandardOutput());
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            fail(QStringLiteral("无法启动内置查询助手运行时：%1")
                     .arg(m_process->errorString()));
        }
    });
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus) {
        if (m_state == State::Stopped || m_state == State::Failed) {
            return;
        }
        fail(QStringLiteral("内置查询助手意外退出（%1）：%2")
                 .arg(exitCode)
                 .arg(m_lastDiagnostic.isEmpty()
                          ? QStringLiteral("没有运行时日志")
                          : m_lastDiagnostic));
    });
}

LocalSearchAssistantRuntime::~LocalSearchAssistantRuntime()
{
    stop();
}

bool LocalSearchAssistantRuntime::start()
{
    if (m_state == State::Ready || m_state == State::Starting) {
        return true;
    }
    m_lastError.clear();
    m_lastDiagnostic.clear();
    m_gpuDeviceName.clear();
    if (!assetsAvailable()) {
        fail(QStringLiteral("内置查询助手资产不完整：运行时=%1，模型=%2")
                 .arg(m_executablePath, m_modelPath));
        return false;
    }
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        if (!m_process->waitForFinished(1000)) {
            fail(QStringLiteral("无法重置内置查询助手运行时"));
            return false;
        }
    }

    QString gpuProbeError;
    m_gpuDeviceName = detectGpuDevice(m_executablePath, &gpuProbeError);
    if (m_gpuDeviceName.isEmpty()) {
        fail(gpuProbeError);
        return false;
    }

    const auto port = reserveLoopbackPort();
    if (port == 0) {
        fail(QStringLiteral("无法为内置查询助手分配本机回环端口"));
        return false;
    }
    m_endpoint = QStringLiteral("http://127.0.0.1:%1").arg(port);
    const auto idealThreads = qMax(1, QThread::idealThreadCount());
    const auto threads = qBound(2, idealThreads / 2, 8);

    m_process->setProgram(QDir::toNativeSeparators(m_executablePath));
    m_process->setWorkingDirectory(QFileInfo(m_executablePath).absolutePath());
    m_process->setArguments({
        QStringLiteral("--model"), QDir::toNativeSeparators(m_modelPath),
        QStringLiteral("--alias"), m_modelName,
        QStringLiteral("--host"), QStringLiteral("127.0.0.1"),
        QStringLiteral("--port"), QString::number(port),
        QStringLiteral("--ctx-size"), QStringLiteral("4096"),
        QStringLiteral("--parallel"), QStringLiteral("1"),
        QStringLiteral("--threads"), QString::number(threads),
        QStringLiteral("--threads-batch"), QString::number(threads),
        QStringLiteral("--n-gpu-layers"), QStringLiteral("99"),
        QStringLiteral("--sleep-idle-seconds"), QStringLiteral("-1"),
        QStringLiteral("--no-webui")
    });
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GGML_LOG_LEVEL"), QStringLiteral("2"));
    m_process->setProcessEnvironment(environment);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_state = State::Starting;
    m_startElapsed.start();
    emit statusChanged();
    m_process->start();
    return true;
}

void LocalSearchAssistantRuntime::stop()
{
    const auto stateChanged = m_state != State::Stopped
        || !m_endpoint.isEmpty()
        || (m_process && m_process->state() != QProcess::NotRunning);
    m_probeTimer.stop();
    m_probeInFlight = false;
    m_state = State::Stopped;
    m_endpoint.clear();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1500)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
    if (stateChanged) {
        emit statusChanged();
    }
}

bool LocalSearchAssistantRuntime::assetsAvailable() const
{
    return QFileInfo(m_executablePath).isFile()
        && QFileInfo(m_modelPath).isFile();
}

bool LocalSearchAssistantRuntime::isReady() const
{
    return m_state == State::Ready;
}

bool LocalSearchAssistantRuntime::isStarting() const
{
    return m_state == State::Starting;
}

QString LocalSearchAssistantRuntime::endpoint() const
{
    return isReady() ? m_endpoint : QString();
}

QString LocalSearchAssistantRuntime::modelName() const
{
    return m_modelName;
}

QString LocalSearchAssistantRuntime::executablePath() const
{
    return m_executablePath;
}

QString LocalSearchAssistantRuntime::modelPath() const
{
    return m_modelPath;
}

QString LocalSearchAssistantRuntime::gpuDeviceName() const
{
    return m_gpuDeviceName;
}

QString LocalSearchAssistantRuntime::lastError() const
{
    return m_lastError;
}

QString LocalSearchAssistantRuntime::defaultExecutablePath()
{
    const auto configured = environmentPath("CINEVAULT_SEARCH_ASSISTANT_RUNTIME");
    if (!configured.isEmpty()) {
        return configured;
    }
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("search-assistant/llama-server.exe"));
}

QString LocalSearchAssistantRuntime::defaultModelPath()
{
    const auto configured = environmentPath("CINEVAULT_SEARCH_ASSISTANT_MODEL");
    if (!configured.isEmpty()) {
        return configured;
    }
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("data/models/qwen3-0.6b/Qwen3-0.6B-Q8_0.gguf"));
}

void LocalSearchAssistantRuntime::probeHealth()
{
    if (m_state != State::Starting || m_probeInFlight) {
        return;
    }
    if (!m_startElapsed.isValid() || m_startElapsed.elapsed() > kStartupTimeoutMs) {
        fail(QStringLiteral("内置查询助手启动超时：%1")
                 .arg(m_lastDiagnostic.isEmpty()
                          ? QStringLiteral("模型尚未完成加载")
                          : m_lastDiagnostic));
        return;
    }

    m_probeInFlight = true;
    QNetworkRequest request{QUrl(m_endpoint + QStringLiteral("/health"))};
    request.setTransferTimeout(1000);
    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_probeInFlight = false;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (m_state != State::Starting || status != 200) {
            return;
        }
        m_probeTimer.stop();
        m_lastError.clear();
        m_state = State::Ready;
        emit statusChanged();
        emit ready();
    });
}

void LocalSearchAssistantRuntime::fail(const QString &message)
{
    if (m_state == State::Failed && m_lastError == message) {
        return;
    }
    m_probeTimer.stop();
    m_probeInFlight = false;
    m_lastError = message.trimmed();
    m_state = State::Failed;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
    emit statusChanged();
    emit failed(m_lastError);
}

void LocalSearchAssistantRuntime::appendDiagnostic(const QByteArray &output)
{
    const auto lines = QString::fromLocal8Bit(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!lines.isEmpty()) {
        m_lastDiagnostic = lines.last().simplified().left(300);
    }
}

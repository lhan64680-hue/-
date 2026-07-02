#include "app/AppBootstrap.h"

#include "app/AppContext.h"
#include "shared/Paths.h"
#include "ui/imaging/LocalImageProvider.h"

#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageLogContext>
#include <QTextStream>

namespace {
QString qmlLogFilePath()
{
    return QDir(Paths::logsRoot()).filePath(QStringLiteral("qml-startup.log"));
}

void appendQmlStartupLog(const QString &message)
{
    QDir().mkpath(Paths::logsRoot());
    QFile file(qmlLogFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << message << "\n";
}

QString messageTypeLabel(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("debug");
    case QtInfoMsg:
        return QStringLiteral("info");
    case QtWarningMsg:
        return QStringLiteral("warning");
    case QtCriticalMsg:
        return QStringLiteral("critical");
    case QtFatalMsg:
        return QStringLiteral("fatal");
    }
    return QStringLiteral("unknown");
}

QtMessageHandler g_previousMessageHandler = nullptr;

void qmlRuntimeMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QStringList parts;
    parts << QStringLiteral("[qt-%1]").arg(messageTypeLabel(type));
    if (context.category && *context.category) {
        parts << QStringLiteral("[category:%1]").arg(QString::fromUtf8(context.category));
    }
    if (context.file && *context.file) {
        parts << QStringLiteral("[file:%1:%2]").arg(QString::fromUtf8(context.file)).arg(context.line);
    }
    parts << message;
    appendQmlStartupLog(parts.join(' '));

    if (g_previousMessageHandler && g_previousMessageHandler != qmlRuntimeMessageHandler) {
        g_previousMessageHandler(type, context, message);
    }
}

void installQmlRuntimeLogger()
{
    static bool installed = false;
    if (installed) {
        return;
    }
    g_previousMessageHandler = qInstallMessageHandler(qmlRuntimeMessageHandler);
    installed = true;
    appendQmlStartupLog(QStringLiteral("[qt-info] Installed Qt/QML runtime message handler."));
}
}

AppBootstrap::AppBootstrap()
    : m_context(std::make_unique<AppContext>())
{
}

AppBootstrap::~AppBootstrap() = default;

bool AppBootstrap::run()
{
    QString errorMessage;
    if (!Paths::ensureBaseDirectories(&errorMessage)) {
        return false;
    }

    installQmlRuntimeLogger();
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_engine->addImageProvider(QStringLiteral("cinevault-local"), new LocalImageProvider);
    QObject::connect(m_engine.get(), &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) {
            appendQmlStartupLog(warning.toString());
        }
    });
    m_context->expose(*m_engine);
    m_engine->loadFromModule("CineVault", "Main");
    const auto loaded = !m_engine->rootObjects().isEmpty();
    if (!loaded) {
        appendQmlStartupLog(QStringLiteral("QML root object load failed."));
    }
    return loaded;
}

#include "app/AppBootstrap.h"
#include "app/AppContext.h"
#include "application/UpdaterSession.h"
#include "ui/widgets/UpdaterWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QTextStream>
#include <QTimer>

#include <algorithm>

namespace {
QString argumentValue(const QStringList &arguments, const QString &prefix)
{
    for (const auto &argument : arguments) {
        if (argument.startsWith(prefix, Qt::CaseInsensitive)) {
            return argument.mid(prefix.size()).trimmed();
        }
    }
    return {};
}

void appendProbeLog(const QString &path, const QString &message)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << ' ' << message << '\n';
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DIT Tools"));
    QApplication::setApplicationName(QStringLiteral("影资管家"));
    QApplication::setApplicationVersion(QStringLiteral(CINEVAULT_APP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));

    const auto arguments = QCoreApplication::arguments();
#if !CINEVAULT_BUILD_MINIMAL_GUI
    const auto probeProjectPath = argumentValue(arguments, QStringLiteral("--analysis-probe-project="));
    const auto probeVideoKey = argumentValue(arguments, QStringLiteral("--analysis-probe-video-key="));
    const auto probeLogPath = argumentValue(arguments, QStringLiteral("--analysis-probe-log="));
    if (!probeProjectPath.isEmpty() || !probeVideoKey.isEmpty()) {
        if (probeProjectPath.isEmpty() || probeVideoKey.isEmpty() || probeLogPath.isEmpty()) {
            return 3;
        }

        AppContext context;
        QObject::connect(&context,
                         &AppContext::analysisProbeProgress,
                         &app,
                         [probeLogPath](qint64 progress,
                                        const QString &detail,
                                        int state,
                                        const QString &errorMessage) {
                             appendProbeLog(probeLogPath,
                                            QStringLiteral("PROGRESS percent=%1 state=%2 detail=%3 error=%4")
                                                .arg(progress)
                                                .arg(state)
                                                .arg(detail)
                                                .arg(errorMessage));
                         });
        QObject::connect(&context,
                         &AppContext::analysisProbeFinished,
                         &app,
                         [probeLogPath](bool success, const QString &message) {
                             appendProbeLog(probeLogPath,
                                            QStringLiteral("FINISHED success=%1 message=%2")
                                                .arg(success ? 1 : 0)
                                                .arg(message));
                             QCoreApplication::exit(success ? 0 : 4);
                         });
        QTimer::singleShot(0, &app, [&context, probeProjectPath, probeVideoKey, probeLogPath]() {
            appendProbeLog(probeLogPath,
                           QStringLiteral("START project=%1 video_key=%2")
                               .arg(probeProjectPath)
                               .arg(probeVideoKey));
            QString errorMessage;
            if (!context.startAnalysisProbe(probeProjectPath, probeVideoKey, &errorMessage)) {
                appendProbeLog(probeLogPath, QStringLiteral("FINISHED success=0 message=%1").arg(errorMessage));
                QCoreApplication::exit(4);
            }
        });
        QTimer::singleShot(2 * 60 * 60 * 1000, &app, [probeLogPath]() {
            appendProbeLog(probeLogPath, QStringLiteral("FINISHED success=0 message=端到端解析测试超时"));
            QCoreApplication::exit(5);
        });
        return app.exec();
    }
#endif
    const auto hasUpdaterSessionArgument = std::any_of(
        arguments.cbegin(), arguments.cend(), [](const QString &argument) {
            return argument.startsWith(QStringLiteral("--run-update-session="), Qt::CaseInsensitive);
        });
    UpdaterInstallSession updaterSession;
    QString updaterArgumentError;
    if (UpdaterSessionRunner::parseArguments(arguments, &updaterSession, &updaterArgumentError)) {
        UpdaterWindow updaterWindow(updaterSession);
        updaterWindow.show();
        updaterWindow.raise();
        updaterWindow.activateWindow();
        return app.exec();
    }
    if (hasUpdaterSessionArgument) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("无法启动更新器"),
                              updaterArgumentError.isEmpty()
                                  ? QStringLiteral("更新会话参数无效。")
                                  : updaterArgumentError);
        return 2;
    }

    AppBootstrap bootstrap;
    if (!bootstrap.run()) {
        return 1;
    }

    return app.exec();
}

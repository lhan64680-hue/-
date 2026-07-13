#include "app/AppBootstrap.h"
#include "application/UpdaterSession.h"
#include "ui/widgets/UpdaterWindow.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>

#include <algorithm>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DIT Tools"));
    QApplication::setApplicationName(QStringLiteral("影资管家"));
    QApplication::setApplicationVersion(QStringLiteral(CINEVAULT_APP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));

    const auto arguments = QCoreApplication::arguments();
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

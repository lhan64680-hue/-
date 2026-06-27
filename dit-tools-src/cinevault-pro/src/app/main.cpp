#include "app/AppBootstrap.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DIT Tools"));
    QApplication::setApplicationName(QStringLiteral("影资管家"));
    QApplication::setApplicationVersion(QStringLiteral(CINEVAULT_APP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));

    AppBootstrap bootstrap;
    if (!bootstrap.run()) {
        return 1;
    }

    return app.exec();
}

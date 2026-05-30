#include "app/AppBootstrap.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DIT Tools"));
    QApplication::setApplicationName(QStringLiteral("影资管家"));

    AppBootstrap bootstrap;
    if (!bootstrap.run()) {
        return 1;
    }

    return app.exec();
}

#include "app/AppBootstrap.h"

#include "app/AppContext.h"
#include "shared/Paths.h"

#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {
void appendQmlStartupLog(const QString &message)
{
    QDir().mkpath(Paths::logsRoot());
    QFile file(QDir(Paths::logsRoot()).filePath(QStringLiteral("qml-startup.log")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << message << "\n";
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

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    m_engine = std::make_unique<QQmlApplicationEngine>();
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

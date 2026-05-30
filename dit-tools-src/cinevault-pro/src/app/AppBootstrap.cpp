#include "app/AppBootstrap.h"

#include "app/AppContext.h"
#include "shared/Paths.h"

#include <QQmlApplicationEngine>
#include <QQuickStyle>

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
    m_context->expose(*m_engine);
    m_engine->loadFromModule("CineVault", "Main");
    return !m_engine->rootObjects().isEmpty();
}

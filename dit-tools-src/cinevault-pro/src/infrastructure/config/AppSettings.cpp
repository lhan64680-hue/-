#include "infrastructure/config/AppSettings.h"

#include <QSettings>

namespace {
constexpr auto kRecentProjectsKey = "recentProjects";
}

AppSettings::AppSettings()
    : m_settings(new QSettings)
{
}

AppSettings::~AppSettings()
{
    delete m_settings;
}

QStringList AppSettings::recentProjects() const
{
    return m_settings->value(QLatin1String(kRecentProjectsKey)).toStringList();
}

void AppSettings::addRecentProject(const QString &projectPath)
{
    auto projects = recentProjects();
    projects.removeAll(projectPath);
    projects.prepend(projectPath);
    while (projects.size() > 10) {
        projects.removeLast();
    }
    m_settings->setValue(QLatin1String(kRecentProjectsKey), projects);
}

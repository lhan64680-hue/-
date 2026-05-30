#pragma once

#include <QStringList>

class QSettings;

class AppSettings {
public:
    AppSettings();
    ~AppSettings();

    QStringList recentProjects() const;
    void addRecentProject(const QString &projectPath);

private:
    QSettings *m_settings = nullptr;
};

#pragma once

#include "domain/Entities.h"

#include <QObject>

class AppSettings;
class DatabaseManager;

class ProjectService : public QObject {
    Q_OBJECT

public:
    explicit ProjectService(DatabaseManager *databaseManager, AppSettings *settings, QObject *parent = nullptr);

    bool createProject(const QString &projectName, const QString &parentDirectory, QString *errorMessage);
    bool openProject(const QString &projectPath, QString *errorMessage);
    void closeProject();

    Project currentProject() const;
    QStringList recentProjects() const;
    bool hasOpenProject() const;

signals:
    void projectChanged();

private:
    bool writeProjectRecord(const Project &project, QString *errorMessage);

    DatabaseManager *m_databaseManager = nullptr;
    AppSettings *m_settings = nullptr;
    Project m_currentProject;
};

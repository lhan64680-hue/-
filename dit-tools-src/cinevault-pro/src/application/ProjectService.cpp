#include "application/ProjectService.h"

#include "infrastructure/config/AppSettings.h"
#include "infrastructure/db/DatabaseManager.h"
#include "infrastructure/logging/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

ProjectService::ProjectService(DatabaseManager *databaseManager, AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_databaseManager(databaseManager)
    , m_settings(settings)
{
}

bool ProjectService::createProject(const QString &projectName, const QString &parentDirectory, QString *errorMessage)
{
    const auto safeName = projectName.trimmed();
    if (safeName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目名称不能为空。");
        }
        return false;
    }

    const auto projectRoot = QDir(parentDirectory).filePath(safeName);
    const auto databasePath = QDir(projectRoot).filePath(QStringLiteral("project.cvdb"));

    QDir dir;
    if (!dir.mkpath(QDir(projectRoot).filePath(QStringLiteral("exports")))
        || !dir.mkpath(QDir(projectRoot).filePath(QStringLiteral("logs")))
        || !dir.mkpath(QDir(projectRoot).filePath(QStringLiteral("reports")))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建项目目录。");
        }
        return false;
    }

    if (!m_databaseManager->openProjectDatabase(databasePath, errorMessage)) {
        return false;
    }

    Project project;
    project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    project.name = safeName;
    project.rootPath = projectRoot;
    project.databasePath = databasePath;
    project.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (!writeProjectRecord(project, errorMessage)) {
        return false;
    }

    m_currentProject = project;
    m_settings->addRecentProject(project.databasePath);
    if (!Logger::initialize(QDir(projectRoot).filePath(QStringLiteral("logs/app.log")), errorMessage)) {
        return false;
    }
    Logger::info(QStringLiteral("项目已创建：%1").arg(project.rootPath));
    emit projectChanged();
    return true;
}

bool ProjectService::openProject(const QString &projectPath, QString *errorMessage)
{
    const QFileInfo info(projectPath);
    const auto databasePath = info.isDir()
        ? QDir(info.absoluteFilePath()).filePath(QStringLiteral("project.cvdb"))
        : info.absoluteFilePath();

    if (!QFileInfo::exists(databasePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未找到项目数据库：%1").arg(databasePath);
        }
        return false;
    }

    if (!m_databaseManager->openProjectDatabase(databasePath, errorMessage)) {
        return false;
    }

    QSqlQuery query(m_databaseManager->database());
    query.prepare(QStringLiteral("SELECT id, name, root_path, created_at FROM project LIMIT 1"));
    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目数据库缺少项目信息。");
        }
        return false;
    }

    m_currentProject.id = query.value(0).toString();
    m_currentProject.name = query.value(1).toString();
    m_currentProject.rootPath = query.value(2).toString();
    m_currentProject.databasePath = databasePath;
    m_currentProject.createdAt = query.value(3).toString();

    m_settings->addRecentProject(databasePath);
    if (!Logger::initialize(QDir(m_currentProject.rootPath).filePath(QStringLiteral("logs/app.log")), errorMessage)) {
        return false;
    }
    Logger::info(QStringLiteral("项目已打开：%1").arg(databasePath));
    emit projectChanged();
    return true;
}

void ProjectService::closeProject()
{
    if (!m_currentProject.databasePath.isEmpty()) {
        Logger::info(QStringLiteral("项目已关闭：%1").arg(m_currentProject.databasePath));
    }
    m_databaseManager->closeProjectDatabase();
    m_currentProject = {};
    emit projectChanged();
}

Project ProjectService::currentProject() const
{
    return m_currentProject;
}

QStringList ProjectService::recentProjects() const
{
    return m_settings->recentProjects();
}

bool ProjectService::hasOpenProject() const
{
    return !m_currentProject.databasePath.isEmpty();
}

bool ProjectService::writeProjectRecord(const Project &project, QString *errorMessage)
{
    QSqlQuery query(m_databaseManager->database());
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO project (id, name, root_path, created_at) VALUES (?, ?, ?, ?)"));
    query.addBindValue(project.id);
    query.addBindValue(project.name);
    query.addBindValue(project.rootPath);
    query.addBindValue(project.createdAt);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

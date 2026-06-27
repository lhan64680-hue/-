#include "application/ProjectService.h"

#include "infrastructure/config/AppSettings.h"
#include "infrastructure/db/DatabaseManager.h"
#include "infrastructure/db/GlobalDatabaseManager.h"
#include "infrastructure/logging/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString projectDatabasePath(const QString &projectPath)
{
    const QFileInfo info(projectPath.trimmed());
    const auto databasePath = info.isDir()
        ? QDir(info.absoluteFilePath()).filePath(QStringLiteral("project.cvdb"))
        : info.absoluteFilePath();
    return QFileInfo(databasePath).absoluteFilePath();
}

QString fallbackProjectName(const QString &databasePath)
{
    const QFileInfo databaseInfo(databasePath);
    const auto rootName = QDir(databaseInfo.absolutePath()).dirName();
    return rootName.isEmpty() ? databaseInfo.completeBaseName() : rootName;
}

bool validateProjectName(const QString &projectName, QString *errorMessage)
{
    const auto safeName = projectName.trimmed();
    if (safeName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目名称不能为空。");
        }
        return false;
    }
    if (safeName == QStringLiteral(".") || safeName == QStringLiteral("..")
        || safeName.contains(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目名称包含非法字符。");
        }
        return false;
    }
    return true;
}

bool ensureProjectDirectories(const QString &projectRoot, QString *errorMessage)
{
    const QStringList directories = {
        QStringLiteral("exports"),
        QStringLiteral("logs"),
        QStringLiteral("reports"),
        QStringLiteral("cache/thumbnails"),
        QStringLiteral("cache/report-preview"),
        QStringLiteral("analysis/frames")
    };

    QDir dir;
    for (const auto &path : directories) {
        const auto absolutePath = QDir(projectRoot).filePath(path);
        if (!dir.mkpath(absolutePath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法创建项目目录：%1").arg(absolutePath);
            }
            return false;
        }
    }
    return true;
}

bool readProjectRecord(const QString &databasePath, Project *project, QString *errorMessage)
{
    if (!project) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目记录为空。");
        }
        return false;
    }

    const auto connectionName = QStringLiteral("cinevault_project_read_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db;
    {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath);
        if (!db.open()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("打开项目数据库失败：%1").arg(db.lastError().text());
            }
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT id, name, root_path, created_at FROM project ORDER BY created_at DESC LIMIT 1"));
        if (!query.exec() || !query.next()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text().isEmpty()
                    ? QStringLiteral("项目数据库缺少项目信息。")
                    : QStringLiteral("读取项目信息失败：%1").arg(query.lastError().text());
            }
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        project->id = query.value(0).toString();
        project->name = query.value(1).toString();
        project->rootPath = query.value(2).toString();
        project->databasePath = databasePath;
        project->createdAt = query.value(3).toString();
    }
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    return true;
}

bool writeProjectRecordToDatabase(const QString &databasePath, const Project &project, QString *errorMessage)
{
    const auto connectionName = QStringLiteral("cinevault_project_write_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db;
    {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath);
        if (!db.open()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("打开项目数据库失败：%1").arg(db.lastError().text());
            }
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        QSqlQuery clear(db);
        if (!clear.exec(QStringLiteral("DELETE FROM project"))) {
            if (errorMessage) {
                *errorMessage = clear.lastError().text();
            }
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        QSqlQuery insert(db);
        insert.prepare(QStringLiteral("INSERT OR REPLACE INTO project (id, name, root_path, created_at) VALUES (?, ?, ?, ?)"));
        insert.addBindValue(project.id);
        insert.addBindValue(project.name);
        insert.addBindValue(project.rootPath);
        insert.addBindValue(project.createdAt);
        if (!insert.exec()) {
            if (errorMessage) {
                *errorMessage = insert.lastError().text();
            }
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }
    }
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    return true;
}

bool samePath(const QString &left, const QString &right)
{
    return QFileInfo(left).absoluteFilePath().compare(QFileInfo(right).absoluteFilePath(), Qt::CaseInsensitive) == 0;
}

bool pathIsInside(const QString &candidate, const QString &root)
{
    auto normalizedCandidate = QDir::cleanPath(QFileInfo(candidate).absoluteFilePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
    auto normalizedRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!normalizedRoot.endsWith(QLatin1Char('/'))) {
        normalizedRoot.append(QLatin1Char('/'));
    }
    return normalizedCandidate.startsWith(normalizedRoot, Qt::CaseInsensitive);
}

bool renameDirectory(const QString &oldRoot, const QString &newRoot, QString *errorMessage)
{
    QDir dir;
    if (dir.rename(oldRoot, newRoot)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("无法移动项目文件夹：%1").arg(oldRoot);
    }
    return false;
}
}

ProjectService::ProjectService(DatabaseManager *databaseManager,
                               AppSettings *settings,
                               GlobalDatabaseManager *globalDatabaseManager,
                               QObject *parent)
    : QObject(parent)
    , m_databaseManager(databaseManager)
    , m_settings(settings)
    , m_globalDatabaseManager(globalDatabaseManager)
{
}

bool ProjectService::createProject(const QString &projectName, const QString &parentDirectory, QString *errorMessage)
{
    const auto safeName = projectName.trimmed();
    if (!validateProjectName(safeName, errorMessage)) {
        return false;
    }
    if (parentDirectory.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目保存目录不能为空。");
        }
        return false;
    }

    const auto projectRoot = QDir(parentDirectory).filePath(safeName);
    const auto databasePath = QDir(projectRoot).filePath(QStringLiteral("project.cvdb"));
    if (QFileInfo::exists(projectRoot)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标项目文件夹已存在：%1").arg(projectRoot);
        }
        return false;
    }

    if (!ensureProjectDirectories(projectRoot, errorMessage)) {
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
    m_settings->addKnownProject(project.databasePath);
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
    const auto databasePath = projectDatabasePath(projectPath);

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
    query.prepare(QStringLiteral("SELECT id, name, root_path, created_at FROM project ORDER BY created_at DESC LIMIT 1"));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取项目信息失败：%1").arg(query.lastError().text());
        }
        m_databaseManager->closeProjectDatabase();
        return false;
    }
    if (!query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目数据库缺少项目信息。");
        }
        m_databaseManager->closeProjectDatabase();
        return false;
    }

    m_currentProject.id = query.value(0).toString();
    m_currentProject.name = query.value(1).toString();
    m_currentProject.rootPath = query.value(2).toString();
    m_currentProject.databasePath = databasePath;
    m_currentProject.createdAt = query.value(3).toString();
    if (!ensureProjectDirectories(m_currentProject.rootPath, errorMessage)) {
        m_databaseManager->closeProjectDatabase();
        m_currentProject = {};
        return false;
    }

    m_settings->addKnownProject(databasePath);
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

QVector<ProjectLibraryEntry> ProjectService::projectLibraryEntries() const
{
    QVector<ProjectLibraryEntry> entries;
    QSet<QString> seen;

    auto appendPath = [this, &entries, &seen](const QString &path) {
        const auto databasePath = projectDatabasePath(path);
        if (databasePath.isEmpty() || seen.contains(databasePath)) {
            return;
        }
        seen.insert(databasePath);
        entries.append(buildProjectLibraryEntry(databasePath));
    };

    for (const auto &path : m_settings->recentProjects()) {
        appendPath(path);
    }
    for (const auto &path : m_settings->knownProjects()) {
        appendPath(path);
    }
    return entries;
}

QStringList ProjectService::recentProjects() const
{
    return m_settings->recentProjects();
}

bool ProjectService::hasOpenProject() const
{
    return !m_currentProject.databasePath.isEmpty();
}

void ProjectService::removeProjectFromLibrary(const QString &projectPath)
{
    m_settings->removeKnownProject(projectDatabasePath(projectPath));
    emit projectLibraryChanged();
}

bool ProjectService::renameProject(const QString &projectPath, const QString &newProjectName, QString *errorMessage)
{
    const auto databasePath = projectDatabasePath(projectPath);
    const auto safeName = newProjectName.trimmed();
    if (!validateProjectName(safeName, errorMessage)) {
        return false;
    }
    if (!QFileInfo::exists(databasePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未找到项目数据库：%1").arg(databasePath);
        }
        return false;
    }

    Project project;
    if (!readProjectRecord(databasePath, &project, errorMessage)) {
        return false;
    }

    const auto oldRoot = QFileInfo(databasePath).absolutePath();
    const auto newRoot = QDir(QFileInfo(oldRoot).absolutePath()).filePath(safeName);
    const auto newDatabasePath = QDir(newRoot).filePath(QStringLiteral("project.cvdb"));
    const bool folderUnchanged = samePath(oldRoot, newRoot);
    if (!folderUnchanged && QFileInfo::exists(newRoot)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标项目文件夹已存在：%1").arg(newRoot);
        }
        return false;
    }

    const bool wasCurrent = !m_currentProject.databasePath.isEmpty() && samePath(m_currentProject.databasePath, databasePath);
    if (wasCurrent) {
        closeProject();
    }

    if (!folderUnchanged && !renameDirectory(oldRoot, newRoot, errorMessage)) {
        if (wasCurrent) {
            QString reopenError;
            openProject(databasePath, &reopenError);
        }
        return false;
    }

    project.name = safeName;
    project.rootPath = newRoot;
    project.databasePath = newDatabasePath;
    if (!ensureProjectDirectories(project.rootPath, errorMessage)
        || !writeProjectRecordToDatabase(project.databasePath, project, errorMessage)) {
        return false;
    }

    m_settings->replaceProjectPath(databasePath, project.databasePath);
    if (m_globalDatabaseManager) {
        m_globalDatabaseManager->updateProjectReference(project.id, project.name, databasePath, project.databasePath, nullptr);
    }

    if (wasCurrent) {
        if (!openProject(project.databasePath, errorMessage)) {
            return false;
        }
    }
    emit projectLibraryChanged();
    return true;
}

bool ProjectService::moveProject(const QString &projectPath, const QString &newParentDirectory, QString *errorMessage)
{
    const auto databasePath = projectDatabasePath(projectPath);
    if (!QFileInfo::exists(databasePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未找到项目数据库：%1").arg(databasePath);
        }
        return false;
    }
    if (newParentDirectory.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标位置不能为空。");
        }
        return false;
    }

    Project project;
    if (!readProjectRecord(databasePath, &project, errorMessage)) {
        return false;
    }

    const auto oldRoot = QFileInfo(databasePath).absolutePath();
    const auto projectFolderName = QFileInfo(oldRoot).fileName();
    const auto targetParent = QFileInfo(newParentDirectory).absoluteFilePath();
    const auto newRoot = QDir(targetParent).filePath(projectFolderName);
    const auto newDatabasePath = QDir(newRoot).filePath(QStringLiteral("project.cvdb"));

    if (samePath(oldRoot, newRoot)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("项目已位于所选位置。");
        }
        return false;
    }
    if (pathIsInside(targetParent, oldRoot)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("不能把项目移动到自身文件夹内部。");
        }
        return false;
    }
    if (QFileInfo::exists(newRoot)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标项目文件夹已存在：%1").arg(newRoot);
        }
        return false;
    }
    QDir dir;
    if (!dir.mkpath(targetParent)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建目标位置：%1").arg(targetParent);
        }
        return false;
    }

    const bool wasCurrent = !m_currentProject.databasePath.isEmpty() && samePath(m_currentProject.databasePath, databasePath);
    if (wasCurrent) {
        closeProject();
    }

    if (!renameDirectory(oldRoot, newRoot, errorMessage)) {
        if (wasCurrent) {
            QString reopenError;
            openProject(databasePath, &reopenError);
        }
        return false;
    }

    project.rootPath = newRoot;
    project.databasePath = newDatabasePath;
    if (!ensureProjectDirectories(project.rootPath, errorMessage)
        || !writeProjectRecordToDatabase(project.databasePath, project, errorMessage)) {
        return false;
    }

    m_settings->replaceProjectPath(databasePath, project.databasePath);
    if (m_globalDatabaseManager) {
        m_globalDatabaseManager->updateProjectReference(project.id, project.name, databasePath, project.databasePath, nullptr);
    }

    if (wasCurrent) {
        if (!openProject(project.databasePath, errorMessage)) {
            return false;
        }
    }
    emit projectLibraryChanged();
    return true;
}

bool ProjectService::deleteProjectToTrash(const QString &projectPath, QString *errorMessage)
{
    const auto databasePath = projectDatabasePath(projectPath);
    if (!QFileInfo::exists(databasePath)) {
        m_settings->removeKnownProject(databasePath);
        if (m_globalDatabaseManager) {
            m_globalDatabaseManager->removeProjectReference(QString(), databasePath, nullptr);
        }
        emit projectLibraryChanged();
        return true;
    }

    Project project;
    if (!readProjectRecord(databasePath, &project, errorMessage)) {
        return false;
    }

    const auto projectRoot = QFileInfo(databasePath).absolutePath();
    const bool wasCurrent = !m_currentProject.databasePath.isEmpty() && samePath(m_currentProject.databasePath, databasePath);
    if (wasCurrent) {
        closeProject();
    }

    QString trashPath;
    if (!QFile::moveToTrash(projectRoot, &trashPath)) {
        if (wasCurrent) {
            QString reopenError;
            openProject(databasePath, &reopenError);
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法将项目移入回收站：%1").arg(projectRoot);
        }
        return false;
    }

    m_settings->removeKnownProject(databasePath);
    if (m_globalDatabaseManager) {
        m_globalDatabaseManager->removeProjectReference(project.id, databasePath, nullptr);
    }
    emit projectLibraryChanged();
    return true;
}

bool ProjectService::writeProjectRecord(const Project &project, QString *errorMessage)
{
    QSqlQuery query(m_databaseManager->database());
    if (!query.exec(QStringLiteral("DELETE FROM project"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

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

ProjectLibraryEntry ProjectService::buildProjectLibraryEntry(const QString &projectPath) const
{
    const auto databasePath = projectDatabasePath(projectPath);
    ProjectLibraryEntry entry;
    entry.databasePath = databasePath;
    entry.rootPath = QFileInfo(databasePath).absolutePath();
    entry.name = fallbackProjectName(databasePath);
    entry.available = QFileInfo::exists(databasePath);
    entry.current = !m_currentProject.databasePath.isEmpty()
        && projectDatabasePath(m_currentProject.databasePath) == databasePath;

    if (!entry.available) {
        return entry;
    }

    const auto connectionName = QStringLiteral("cinevault_project_library_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db;
    {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath);
        if (db.open()) {
            QSqlQuery query(db);
            query.prepare(QStringLiteral("SELECT name, root_path, created_at FROM project ORDER BY created_at DESC LIMIT 1"));
            if (query.exec() && query.next()) {
                entry.name = query.value(0).toString();
                entry.rootPath = query.value(1).toString();
                entry.createdAt = query.value(2).toString();
            }
        }
        db.close();
    }
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

    return entry;
}

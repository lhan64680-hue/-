#include "infrastructure/db/GlobalDatabaseManager.h"

#include "shared/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace {
bool executeBatch(QSqlDatabase &db, const QStringList &statements, QString *errorMessage)
{
    QSqlQuery query(db);
    for (const auto &statement : statements) {
        if (!query.exec(statement)) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }
    return true;
}
}

GlobalDatabaseManager::GlobalDatabaseManager(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("cinevault_global"))
{
}

bool GlobalDatabaseManager::openDatabase(QString *errorMessage)
{
    closeDatabase();

    QString pathError;
    if (!Paths::ensureBaseDirectories(&pathError)) {
        if (errorMessage) {
            *errorMessage = pathError;
        }
        return false;
    }

    m_databaseFilePath = QDir(Paths::resolvedDataRoot()).filePath(QStringLiteral("material-center.sqlite"));

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_databaseFilePath);
    if (!db.open()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
        closeDatabase();
        return false;
    }

    if (!initializeSchema(db, errorMessage)) {
        closeDatabase();
        return false;
    }

    return true;
}

void GlobalDatabaseManager::closeDatabase()
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        m_databaseFilePath.clear();
        m_hasFts5 = false;
        return;
    }

    {
        auto db = QSqlDatabase::database(m_connectionName);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_databaseFilePath.clear();
    m_hasFts5 = false;
}

bool GlobalDatabaseManager::isOpen() const
{
    return !m_databaseFilePath.isEmpty() && QSqlDatabase::contains(m_connectionName);
}

bool GlobalDatabaseManager::hasFts5() const
{
    return m_hasFts5;
}

QString GlobalDatabaseManager::databaseFilePath() const
{
    return m_databaseFilePath;
}

QSqlDatabase GlobalDatabaseManager::database() const
{
    return QSqlDatabase::database(m_connectionName);
}

QSqlDatabase GlobalDatabaseManager::openThreadConnection(const QString &connectionName, QString *errorMessage) const
{
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_databaseFilePath);
    if (!db.open()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
    }
    return db;
}

void GlobalDatabaseManager::closeThreadConnection(const QString &connectionName) const
{
    if (!QSqlDatabase::contains(connectionName)) {
        return;
    }
    {
        auto db = QSqlDatabase::database(connectionName);
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

bool GlobalDatabaseManager::updateProjectReference(const QString &projectUuid,
                                                   const QString &projectName,
                                                   const QString &oldDatabasePath,
                                                   const QString &newDatabasePath,
                                                   QString *errorMessage)
{
    if (!isOpen()) {
        return true;
    }

    auto db = database();
    if (!db.transaction()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    if (m_hasFts5) {
        QSqlQuery updateFts(db);
        updateFts.prepare(QStringLiteral(
            "UPDATE video_search_fts SET project_name = ? "
            "WHERE video_key IN ("
            "SELECT video_key FROM global_video_asset WHERE project_uuid = ? OR project_database_path = ?"
            ")"));
        updateFts.addBindValue(projectName);
        updateFts.addBindValue(projectUuid);
        updateFts.addBindValue(oldDatabasePath);
        if (!updateFts.exec()) {
            if (errorMessage) {
                *errorMessage = updateFts.lastError().text();
            }
            db.rollback();
            return false;
        }
    }

    QSqlQuery updateAssets(db);
    updateAssets.prepare(QStringLiteral(
        "UPDATE global_video_asset SET project_name = ?, project_database_path = ?, updated_at = ? "
        "WHERE project_uuid = ? OR project_database_path = ?"));
    updateAssets.addBindValue(projectName);
    updateAssets.addBindValue(newDatabasePath);
    updateAssets.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    updateAssets.addBindValue(projectUuid);
    updateAssets.addBindValue(oldDatabasePath);
    if (!updateAssets.exec()) {
        if (errorMessage) {
            *errorMessage = updateAssets.lastError().text();
        }
        db.rollback();
        return false;
    }

    QSqlQuery updateRegistry(db);
    updateRegistry.prepare(QStringLiteral(
        "UPDATE project_registry SET project_name = ?, project_database_path = ?, last_synced_at = ?, error_message = '' "
        "WHERE project_uuid = ? OR project_database_path = ?"));
    updateRegistry.addBindValue(projectName);
    updateRegistry.addBindValue(newDatabasePath);
    updateRegistry.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    updateRegistry.addBindValue(projectUuid);
    updateRegistry.addBindValue(oldDatabasePath);
    if (!updateRegistry.exec()) {
        if (errorMessage) {
            *errorMessage = updateRegistry.lastError().text();
        }
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
        db.rollback();
        return false;
    }
    return true;
}

bool GlobalDatabaseManager::removeProjectReference(const QString &projectUuid, const QString &databasePath, QString *errorMessage)
{
    if (!isOpen()) {
        return true;
    }

    auto db = database();
    if (!db.transaction()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    if (m_hasFts5) {
        QSqlQuery deleteFts(db);
        deleteFts.prepare(QStringLiteral(
            "DELETE FROM video_search_fts WHERE video_key IN ("
            "SELECT video_key FROM global_video_asset WHERE project_uuid = ? OR project_database_path = ?"
            ")"));
        deleteFts.addBindValue(projectUuid);
        deleteFts.addBindValue(databasePath);
        if (!deleteFts.exec()) {
            if (errorMessage) {
                *errorMessage = deleteFts.lastError().text();
            }
            db.rollback();
            return false;
        }
    }

    QSqlQuery deleteAssets(db);
    deleteAssets.prepare(QStringLiteral("DELETE FROM global_video_asset WHERE project_uuid = ? OR project_database_path = ?"));
    deleteAssets.addBindValue(projectUuid);
    deleteAssets.addBindValue(databasePath);
    if (!deleteAssets.exec()) {
        if (errorMessage) {
            *errorMessage = deleteAssets.lastError().text();
        }
        db.rollback();
        return false;
    }

    QSqlQuery deleteRegistry(db);
    deleteRegistry.prepare(QStringLiteral("DELETE FROM project_registry WHERE project_uuid = ? OR project_database_path = ?"));
    deleteRegistry.addBindValue(projectUuid);
    deleteRegistry.addBindValue(databasePath);
    if (!deleteRegistry.exec()) {
        if (errorMessage) {
            *errorMessage = deleteRegistry.lastError().text();
        }
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
        db.rollback();
        return false;
    }
    return true;
}

bool GlobalDatabaseManager::initializeSchema(QSqlDatabase &db, QString *errorMessage)
{
    if (!createSchema(db, errorMessage)) {
        return false;
    }

    auto version = currentSchemaVersion(db);
    if (version < 1) {
        if (!setSchemaVersion(db, 1, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool GlobalDatabaseManager::createSchema(QSqlDatabase &db, QString *errorMessage)
{
    const QStringList statements = {
        QStringLiteral("PRAGMA journal_mode=WAL;"),
        QStringLiteral("PRAGMA synchronous=NORMAL;"),
        QStringLiteral("PRAGMA foreign_keys=ON;"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);"),
        QStringLiteral("INSERT INTO schema_version(version) SELECT 1 WHERE NOT EXISTS (SELECT 1 FROM schema_version);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS project_registry ("
                       "project_uuid TEXT PRIMARY KEY,"
                       "project_name TEXT NOT NULL,"
                       "project_database_path TEXT NOT NULL,"
                       "last_synced_at TEXT,"
                       "sync_status TEXT NOT NULL DEFAULT 'pending',"
                       "error_message TEXT"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS global_video_asset ("
                       "video_key TEXT PRIMARY KEY,"
                       "project_uuid TEXT NOT NULL,"
                       "project_name TEXT NOT NULL,"
                       "project_database_path TEXT NOT NULL,"
                       "source_root_id INTEGER NOT NULL DEFAULT 0,"
                       "source_root_name TEXT NOT NULL DEFAULT '',"
                       "asset_id INTEGER NOT NULL,"
                       "file_name TEXT NOT NULL,"
                       "absolute_path TEXT NOT NULL,"
                       "relative_path TEXT NOT NULL,"
                       "size_bytes INTEGER NOT NULL DEFAULT 0,"
                       "modified_at TEXT NOT NULL DEFAULT '',"
                       "duration_ms INTEGER NOT NULL DEFAULT 0,"
                       "thumbnail_path TEXT,"
                       "analysis_status INTEGER NOT NULL DEFAULT 0,"
                       "confirmation_status INTEGER NOT NULL DEFAULT 0,"
                       "error_message TEXT,"
                       "last_synced_at TEXT NOT NULL DEFAULT '',"
                       "updated_at TEXT NOT NULL DEFAULT '',"
                       "FOREIGN KEY(project_uuid) REFERENCES project_registry(project_uuid) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS video_analysis_result ("
                       "video_key TEXT PRIMARY KEY,"
                       "summary TEXT,"
                       "keywords_json TEXT,"
                       "scenes_json TEXT,"
                       "search_text TEXT,"
                       "model_name TEXT,"
                       "prompt_version TEXT,"
                       "analyzed_at TEXT,"
                       "confirmed_at TEXT,"
                       "FOREIGN KEY(video_key) REFERENCES global_video_asset(video_key) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS video_frame_analysis ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "video_key TEXT NOT NULL,"
                       "frame_number INTEGER NOT NULL DEFAULT 0,"
                       "timestamp_ms INTEGER NOT NULL DEFAULT 0,"
                       "image_path TEXT,"
                       "caption TEXT,"
                       "tags_json TEXT,"
                       "objects_json TEXT,"
                       "actions TEXT,"
                       "setting_text TEXT,"
                       "error_message TEXT,"
                       "FOREIGN KEY(video_key) REFERENCES global_video_asset(video_key) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_global_video_project ON global_video_asset(project_uuid);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_global_video_source ON global_video_asset(source_root_name);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_global_video_status ON global_video_asset(analysis_status, confirmation_status);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_frame_video_key ON video_frame_analysis(video_key);")
    };

    if (!executeBatch(db, statements, errorMessage)) {
        return false;
    }

    QSqlQuery query(db);
    m_hasFts5 = query.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS video_search_fts USING fts5("
        "video_key UNINDEXED,"
        "project_name,"
        "source_root_name,"
        "file_name,"
        "relative_path,"
        "summary,"
        "keywords,"
        "captions,"
        "tokenize='unicode61'"
        ");"));
    if (!m_hasFts5) {
        query.exec(QStringLiteral("DROP TABLE IF EXISTS video_search_fts"));
    }
    return true;
}

int GlobalDatabaseManager::currentSchemaVersion(QSqlDatabase &db) const
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))) {
        return 0;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

bool GlobalDatabaseManager::setSchemaVersion(QSqlDatabase &db, int version, QString *errorMessage) const
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("DELETE FROM schema_version"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    query.prepare(QStringLiteral("INSERT INTO schema_version(version) VALUES (?)"));
    query.addBindValue(version);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

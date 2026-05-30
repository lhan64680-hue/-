#include "infrastructure/db/DatabaseManager.h"

#include <QFileInfo>
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

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , m_mainConnectionName(QStringLiteral("cinevault_main"))
{
}

bool DatabaseManager::openProjectDatabase(const QString &databaseFilePath, QString *errorMessage)
{
    closeProjectDatabase();

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_mainConnectionName);
    db.setDatabaseName(databaseFilePath);
    if (!db.open()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    if (!initializeSchema(db, errorMessage)) {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_mainConnectionName);
        return false;
    }

    m_databaseFilePath = databaseFilePath;
    return true;
}

void DatabaseManager::closeProjectDatabase()
{
    if (!QSqlDatabase::contains(m_mainConnectionName)) {
        m_databaseFilePath.clear();
        m_schemaVersion = 0;
        return;
    }

    {
        auto db = QSqlDatabase::database(m_mainConnectionName);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_mainConnectionName);
    m_databaseFilePath.clear();
    m_schemaVersion = 0;
}

bool DatabaseManager::hasOpenProject() const
{
    return !m_databaseFilePath.isEmpty() && QSqlDatabase::contains(m_mainConnectionName);
}

QString DatabaseManager::databaseFilePath() const
{
    return m_databaseFilePath;
}

int DatabaseManager::schemaVersion() const
{
    return m_schemaVersion;
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(m_mainConnectionName);
}

QSqlDatabase DatabaseManager::openThreadConnection(const QString &connectionName, QString *errorMessage) const
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

void DatabaseManager::closeThreadConnection(const QString &connectionName) const
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

bool DatabaseManager::initializeSchema(QSqlDatabase &db, QString *errorMessage)
{
    if (!createBaseSchema(db, errorMessage)) {
        return false;
    }

    auto version = currentSchemaVersion(db);
    if (version < 1) {
        version = 1;
        if (!setSchemaVersion(db, version, errorMessage)) {
            return false;
        }
    }

    if (version < 2) {
        if (!migrateToVersion2(db, errorMessage)) {
            return false;
        }
        version = 2;
    }

    m_schemaVersion = version;
    return true;
}

bool DatabaseManager::createBaseSchema(QSqlDatabase &db, QString *errorMessage) const
{
    const QStringList statements = {
        QStringLiteral("PRAGMA journal_mode=WAL;"),
        QStringLiteral("PRAGMA synchronous=NORMAL;"),
        QStringLiteral("PRAGMA foreign_keys=ON;"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);"),
        QStringLiteral("INSERT INTO schema_version(version) SELECT 1 WHERE NOT EXISTS (SELECT 1 FROM schema_version);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS project (id TEXT PRIMARY KEY, name TEXT NOT NULL, root_path TEXT NOT NULL, created_at TEXT NOT NULL);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS source_root ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "name TEXT NOT NULL,"
                       "path TEXT NOT NULL UNIQUE,"
                       "status TEXT NOT NULL,"
                       "total_files INTEGER NOT NULL DEFAULT 0,"
                       "total_folders INTEGER NOT NULL DEFAULT 0,"
                       "total_size_bytes INTEGER NOT NULL DEFAULT 0,"
                       "video_count INTEGER NOT NULL DEFAULT 0,"
                       "audio_count INTEGER NOT NULL DEFAULT 0,"
                       "image_count INTEGER NOT NULL DEFAULT 0,"
                       "other_count INTEGER NOT NULL DEFAULT 0,"
                       "warning_count INTEGER NOT NULL DEFAULT 0,"
                       "created_at TEXT NOT NULL,"
                       "updated_at TEXT NOT NULL"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS folder_node ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "source_root_id INTEGER NOT NULL,"
                       "absolute_path TEXT NOT NULL,"
                       "relative_path TEXT NOT NULL,"
                       "file_count INTEGER NOT NULL DEFAULT 0,"
                       "created_at TEXT NOT NULL,"
                       "FOREIGN KEY(source_root_id) REFERENCES source_root(id) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS asset_file ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "source_root_id INTEGER NOT NULL,"
                       "name TEXT NOT NULL,"
                       "extension TEXT,"
                       "absolute_path TEXT NOT NULL,"
                       "relative_path TEXT NOT NULL,"
                       "parent_path TEXT NOT NULL,"
                       "asset_type INTEGER NOT NULL,"
                       "size_bytes INTEGER NOT NULL DEFAULT 0,"
                       "modified_at TEXT NOT NULL,"
                       "is_readable INTEGER NOT NULL DEFAULT 0,"
                       "created_at TEXT NOT NULL,"
                       "FOREIGN KEY(source_root_id) REFERENCES source_root(id) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS job ("
                       "id INTEGER PRIMARY KEY,"
                       "type INTEGER NOT NULL,"
                       "state INTEGER NOT NULL,"
                       "title TEXT NOT NULL,"
                       "detail TEXT,"
                       "error_message TEXT,"
                       "progress INTEGER NOT NULL DEFAULT 0,"
                       "source_root_id INTEGER NOT NULL DEFAULT 0,"
                       "started_at TEXT NOT NULL,"
                       "updated_at TEXT NOT NULL"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS app_log ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "level TEXT NOT NULL,"
                       "message TEXT NOT NULL,"
                       "created_at TEXT NOT NULL"
                       ");"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_asset_file_source_root_id ON asset_file(source_root_id);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_asset_file_name ON asset_file(name);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_folder_node_source_root_id ON folder_node(source_root_id);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_source_root_status ON source_root(status);")
    };

    return executeBatch(db, statements, errorMessage);
}

bool DatabaseManager::migrateToVersion2(QSqlDatabase &db, QString *errorMessage) const
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS media_metadata ("
                       "asset_id INTEGER PRIMARY KEY,"
                       "probe_status INTEGER NOT NULL DEFAULT 0,"
                       "media_type INTEGER NOT NULL DEFAULT 0,"
                       "container TEXT,"
                       "duration_ms INTEGER NOT NULL DEFAULT 0,"
                       "bit_rate INTEGER NOT NULL DEFAULT 0,"
                       "raw_json TEXT,"
                       "error_message TEXT,"
                       "updated_at TEXT NOT NULL,"
                       "FOREIGN KEY(asset_id) REFERENCES asset_file(id) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS media_stream ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "asset_id INTEGER NOT NULL,"
                       "stream_index INTEGER NOT NULL,"
                       "stream_kind TEXT NOT NULL,"
                       "codec TEXT,"
                       "bit_rate INTEGER NOT NULL DEFAULT 0,"
                       "width INTEGER NOT NULL DEFAULT 0,"
                       "height INTEGER NOT NULL DEFAULT 0,"
                       "channels INTEGER NOT NULL DEFAULT 0,"
                       "sample_rate INTEGER NOT NULL DEFAULT 0,"
                       "FOREIGN KEY(asset_id) REFERENCES media_metadata(asset_id) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS thumbnail ("
                       "asset_id INTEGER PRIMARY KEY,"
                       "status INTEGER NOT NULL DEFAULT 0,"
                       "image_path TEXT,"
                       "updated_at TEXT NOT NULL,"
                       "error_message TEXT,"
                       "FOREIGN KEY(asset_id) REFERENCES asset_file(id) ON DELETE CASCADE"
                       ");"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_media_stream_asset_id ON media_stream(asset_id);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_media_metadata_probe_status ON media_metadata(probe_status);")
    };

    if (!executeBatch(db, statements, errorMessage)) {
        return false;
    }

    return setSchemaVersion(db, 2, errorMessage);
}

int DatabaseManager::currentSchemaVersion(QSqlDatabase &db) const
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))) {
        return 0;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

bool DatabaseManager::setSchemaVersion(QSqlDatabase &db, int version, QString *errorMessage) const
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM schema_version"));
    if (!query.exec()) {
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

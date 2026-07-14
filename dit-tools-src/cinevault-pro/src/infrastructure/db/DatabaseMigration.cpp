#include "infrastructure/db/DatabaseMigration.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
bool executeStatement(QSqlDatabase &db, const QString &statement, QString *errorMessage)
{
    QSqlQuery query(db);
    if (query.exec(statement)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}
}

bool DatabaseMigration::configureSqlite(QSqlDatabase &db, QString *errorMessage)
{
    return executeStatement(db, QStringLiteral("PRAGMA journal_mode=WAL;"), errorMessage)
        && executeStatement(db, QStringLiteral("PRAGMA synchronous=NORMAL;"), errorMessage)
        && executeStatement(db, QStringLiteral("PRAGMA foreign_keys=ON;"), errorMessage);
}

QString DatabaseMigration::backupFilePath(const QString &databaseFilePath, int targetVersion)
{
    return QStringLiteral("%1.pre-v%2.bak").arg(databaseFilePath).arg(targetVersion);
}

bool DatabaseMigration::createUpgradeBackup(QSqlDatabase &db,
                                            int targetVersion,
                                            bool databaseExistedBeforeOpen,
                                            QString *errorMessage)
{
    if (!databaseExistedBeforeOpen || db.databaseName().isEmpty() || db.databaseName() == QStringLiteral(":memory:")) {
        return true;
    }

    const auto backupPath = backupFilePath(db.databaseName(), targetVersion);
    const QFileInfo existingBackup(backupPath);
    if (existingBackup.exists()) {
        if (existingBackup.isFile() && existingBackup.size() > 0) {
            return true;
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("升级备份路径不可用：%1").arg(backupPath);
        }
        return false;
    }

    auto escapedPath = backupPath;
    escapedPath.replace(QLatin1Char('\''), QStringLiteral("''"));
    if (!executeStatement(db, QStringLiteral("VACUUM INTO '%1'").arg(escapedPath), errorMessage)) {
        if (errorMessage && !errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("创建升级前备份失败：%1，%2").arg(backupPath, *errorMessage);
        }
        return false;
    }
    return true;
}

#pragma once

#include <QString>

class QSqlDatabase;

namespace DatabaseMigration {

bool configureSqlite(QSqlDatabase &db, QString *errorMessage);
QString backupFilePath(const QString &databaseFilePath, int targetVersion);
bool createUpgradeBackup(QSqlDatabase &db,
                         int targetVersion,
                         bool databaseExistedBeforeOpen,
                         QString *errorMessage);

}

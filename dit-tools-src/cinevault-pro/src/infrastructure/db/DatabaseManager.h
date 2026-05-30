#pragma once

#include <QObject>
#include <QString>

class QSqlDatabase;

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);

    bool openProjectDatabase(const QString &databaseFilePath, QString *errorMessage);
    void closeProjectDatabase();
    bool hasOpenProject() const;
    QString databaseFilePath() const;
    int schemaVersion() const;
    QSqlDatabase database() const;
    QSqlDatabase openThreadConnection(const QString &connectionName, QString *errorMessage) const;
    void closeThreadConnection(const QString &connectionName) const;

private:
    bool initializeSchema(QSqlDatabase &db, QString *errorMessage);
    bool createBaseSchema(QSqlDatabase &db, QString *errorMessage) const;
    bool migrateToVersion2(QSqlDatabase &db, QString *errorMessage) const;
    int currentSchemaVersion(QSqlDatabase &db) const;
    bool setSchemaVersion(QSqlDatabase &db, int version, QString *errorMessage) const;

    QString m_mainConnectionName;
    QString m_databaseFilePath;
    int m_schemaVersion = 0;
};

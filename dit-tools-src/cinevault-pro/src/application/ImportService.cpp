#include "application/ImportService.h"

#include "application/JobService.h"
#include "core/jobs/JobEngine.h"
#include "core/scan/ScanEngine.h"
#include "infrastructure/db/DatabaseManager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

ImportService::ImportService(DatabaseManager *databaseManager, JobService *jobService, ScanEngine *scanEngine, QObject *parent)
    : QObject(parent)
    , m_databaseManager(databaseManager)
    , m_jobService(jobService)
    , m_scanEngine(scanEngine)
{
    connect(m_scanEngine, &ScanEngine::scanBatchCommitted, this, &ImportService::catalogChanged);
    connect(m_scanEngine, &ScanEngine::scanFinished, this, &ImportService::catalogChanged);
    connect(m_scanEngine, &ScanEngine::scanFailed, this, [this](qint64, const QString &message) {
        m_lastMessage = message;
        emit importStateChanged();
        emit catalogChanged();
    });
}

bool ImportService::importDirectory(const QString &directoryPath, QString *errorMessage)
{
    const QFileInfo info(directoryPath);
    if (!info.exists() || !info.isDir()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目录不存在：%1").arg(directoryPath);
        }
        return false;
    }
    if (!info.isReadable()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目录不可读：%1").arg(directoryPath);
        }
        return false;
    }

    QSqlQuery duplicateQuery(m_databaseManager->database());
    duplicateQuery.prepare(QStringLiteral("SELECT id FROM source_root WHERE path = ?"));
    duplicateQuery.addBindValue(info.absoluteFilePath());
    if (duplicateQuery.exec() && duplicateQuery.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("素材源已存在：%1").arg(info.absoluteFilePath());
        }
        return false;
    }

    const auto now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery insertQuery(m_databaseManager->database());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO source_root (name, path, status, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?)"));
    insertQuery.addBindValue(info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName());
    insertQuery.addBindValue(info.absoluteFilePath());
    insertQuery.addBindValue(QStringLiteral("scanning"));
    insertQuery.addBindValue(now);
    insertQuery.addBindValue(now);
    if (!insertQuery.exec()) {
        if (errorMessage) {
            *errorMessage = insertQuery.lastError().text();
        }
        return false;
    }

    SourceRoot sourceRoot;
    sourceRoot.id = insertQuery.lastInsertId().toLongLong();
    sourceRoot.name = info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName();
    sourceRoot.path = info.absoluteFilePath();
    sourceRoot.status = QStringLiteral("scanning");

    const auto jobId = m_jobService->engine()->createJob(JobType::Scan, QStringLiteral("扫描 %1").arg(sourceRoot.name), QStringLiteral("准备扫描目录"), sourceRoot.id);
    m_scanEngine->startScan(sourceRoot, jobId);

    m_lastMessage = QStringLiteral("已开始导入素材源：%1").arg(sourceRoot.name);
    emit importStateChanged();
    emit catalogChanged();
    return true;
}

QString ImportService::lastMessage() const
{
    return m_lastMessage;
}

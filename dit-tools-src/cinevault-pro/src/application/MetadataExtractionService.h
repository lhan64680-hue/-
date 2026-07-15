#pragma once

#include "domain/Entities.h"
#include "infrastructure/metadata/ExifToolAdapter.h"

#include <QObject>
#include <QSet>

class DatabaseManager;
class JobEngine;
class QSqlDatabase;

class MetadataExtractionService final : public QObject {
    Q_OBJECT

public:
    explicit MetadataExtractionService(DatabaseManager *databaseManager,
                                       JobEngine *jobEngine,
                                       ExifToolAdapter *exifToolAdapter,
                                       QObject *parent = nullptr);

public slots:
    void startForSourceRoot(qint64 sourceRootId);

signals:
    void metadataCatalogChanged();

private:
    QVector<AssetFile> fetchPendingAssets(QSqlDatabase &db,
                                          qint64 sourceRootId,
                                          QString *errorMessage) const;
    void runExtraction(qint64 sourceRootId,
                       const QString &projectDatabasePath,
                       const QString &activeKey,
                       qint64 jobId);
    bool persistBatch(QSqlDatabase &db,
                      const QVector<EmbeddedMetadataResult> &results,
                      QString *errorMessage) const;
    void updateJob(qint64 jobId,
                   qint64 progress,
                   const QString &detail,
                   const JobProgressContext &context);
    void completeJob(qint64 jobId, const QString &detail);
    void failJob(qint64 jobId, const QString &message);
    void releaseActiveKey(const QString &activeKey);

    DatabaseManager *m_databaseManager = nullptr;
    JobEngine *m_jobEngine = nullptr;
    ExifToolAdapter *m_exifToolAdapter = nullptr;
    QSet<QString> m_activeKeys;
};

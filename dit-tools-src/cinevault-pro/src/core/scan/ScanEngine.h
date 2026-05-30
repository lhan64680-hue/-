#pragma once

#include "domain/Entities.h"

#include <QObject>

class DatabaseManager;
class JobEngine;

class ScanEngine : public QObject {
    Q_OBJECT

public:
    explicit ScanEngine(DatabaseManager *databaseManager, JobEngine *jobEngine, QObject *parent = nullptr);

    void startScan(const SourceRoot &sourceRoot, qint64 jobId);

signals:
    void scanBatchCommitted(const ScanBatch &batch);
    void scanFinished(qint64 sourceRootId);
    void scanFailed(qint64 sourceRootId, const QString &message);

private:
    void runScan(SourceRoot sourceRoot, qint64 jobId);

    DatabaseManager *m_databaseManager = nullptr;
    JobEngine *m_jobEngine = nullptr;
};

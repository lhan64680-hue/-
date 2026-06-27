#pragma once

#include "domain/Entities.h"

#include <QObject>

class JobEngine;
class BackupCopyEngine;

class MaterialBackupService : public QObject {
    Q_OBJECT

public:
    explicit MaterialBackupService(JobEngine *jobEngine, QObject *parent = nullptr);

    BackupPlan buildPlan(const BackupRequest &request) const;
    bool isRunning() const;

public slots:
    bool startBackup(const BackupPlan &plan);
    void cancelBackup();

signals:
    void backupStarted();
    void backupTaskUpdated(const BackupDestinationTask &task);
    void backupFinished(const BackupExecutionResult &result);
    void messageChanged(const QString &message);

private:
    JobEngine *m_jobEngine = nullptr;
    BackupCopyEngine *m_copyEngine = nullptr;
    bool m_running = false;
};

#pragma once

#include "domain/Entities.h"

#include <QObject>

#include <atomic>
#include <functional>

class VolumeEjectService;

class BackupCopyEngine : public QObject {
    Q_OBJECT

public:
    using ProgressCallback = std::function<void(const BackupDestinationTask &)>;

    explicit BackupCopyEngine(QObject *parent = nullptr);

    BackupExecutionResult copyDirect(const BackupPlan &plan, const ProgressCallback &progressCallback = {});
    BackupExecutionResult copyCascade(const BackupPlan &plan,
                                      VolumeEjectService *volumeEjectService = nullptr,
                                      const ProgressCallback &progressCallback = {});
    void requestCancel();
    void resetCancel();
    bool isCancelRequested() const;

private:
    BackupDestinationTask copyDestination(const BackupPlan &plan,
                                          BackupDestinationTask task,
                                          const ProgressCallback &progressCallback) const;
    BackupExecutionResult copyTasksParallel(const BackupPlan &plan,
                                            const QVector<BackupDestinationTask> &tasks,
                                            const ProgressCallback &progressCallback) const;

    std::atomic_bool m_cancelRequested = false;
};

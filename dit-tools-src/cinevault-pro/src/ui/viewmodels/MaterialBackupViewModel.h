#pragma once

#include "domain/Entities.h"

#include <QDateTime>
#include <QObject>
#include <QVariantList>

class AppSettings;
class BackupDestinationListModel;
class BackupSourceListModel;
class BackupTaskListModel;
class ImportService;
class MaterialBackupService;
class ProjectService;
class QJsonObject;

class MaterialBackupViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString summaryText READ summaryText NOTIFY stateChanged)
    Q_PROPERTY(bool hasOpenProject READ hasOpenProject NOTIFY stateChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY stateChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool canStartBackup READ canStartBackup NOTIFY stateChanged)
    Q_PROPERTY(bool canAddBackupTask READ canAddBackupTask NOTIFY stateChanged)
    Q_PROPERTY(int queuedTaskCount READ queuedTaskCount NOTIFY stateChanged)
    Q_PROPERTY(bool cascadeEnabled READ cascadeEnabled WRITE setCascadeEnabled NOTIFY stateChanged)
    Q_PROPERTY(int verificationMode READ verificationMode WRITE setVerificationMode NOTIFY stateChanged)
    Q_PROPERTY(QVariantList verificationOptions READ verificationOptions CONSTANT)
    Q_PROPERTY(int overallProgress READ overallProgress NOTIFY stateChanged)
    Q_PROPERTY(QObject *sourceModel READ sourceModel CONSTANT)
    Q_PROPERTY(QObject *destinationModel READ destinationModel CONSTANT)
    Q_PROPERTY(QObject *taskModel READ taskModel CONSTANT)

public:
    explicit MaterialBackupViewModel(ProjectService *projectService,
                                     MaterialBackupService *backupService,
                                     ImportService *importService,
                                     AppSettings *settings,
                                     QObject *parent = nullptr);

    QString summaryText() const;
    bool hasOpenProject() const;
    QString projectPath() const;
    QString lastMessage() const;
    bool running() const;
    bool canStartBackup() const;
    bool canAddBackupTask() const;
    int queuedTaskCount() const;
    bool cascadeEnabled() const;
    int verificationMode() const;
    QVariantList verificationOptions() const;
    int overallProgress() const;
    QObject *sourceModel() const;
    QObject *destinationModel() const;
    QObject *taskModel() const;

    void setCascadeEnabled(bool enabled);
    void setVerificationMode(int mode);

    Q_INVOKABLE void addFileSources();
    Q_INVOKABLE void addFolderSource();
    Q_INVOKABLE void addVolumeSource();
    Q_INVOKABLE void addDestination();
    Q_INVOKABLE void removeSource(int row);
    Q_INVOKABLE void removeDestination(int row);
    Q_INVOKABLE void setPrimaryDestination(int row);
    Q_INVOKABLE void enqueueBackupTask();
    Q_INVOKABLE void removeQueuedTask(const QString &taskId);
    Q_INVOKABLE void startBackup();
    Q_INVOKABLE void cancelBackup();

signals:
    void stateChanged();

private:
    struct QueuedBackupTask {
        QString id;
        QString batchName;
        QString createdAt;
        QVector<BackupSource> sources;
        QVector<BackupDestination> destinations;
        BackupVerificationMode verificationMode = BackupVerificationMode::Off;
        bool cascadeEnabled = false;
        int primaryDestinationIndex = 0;
        qint64 totalFiles = 0;
        qint64 totalBytes = 0;
        QString plannedRootPath;
    };

    void resetForProject();
    void loadQueueForProject();
    void persistQueue() const;
    void rebuildTaskModel();
    void refreshPlan();
    void recomputeOverallProgress();
    void addSourcePath(const QString &path, BackupSourceKind kind);
    void clearDraft();
    void startNextQueuedTask();
    void finishActiveQueuedTask(const BackupExecutionResult &result);
    void updateActiveDestinationTask(const BackupDestinationTask &task);
    BackupDestinationTask taskRowForQueuedTask(const QueuedBackupTask &task) const;
    BackupDestinationTask activeTaskRow(BackupTaskState fallbackState, const QString &statusText, const QString &errorMessage) const;
    BackupRequest requestForQueuedTask(const QueuedBackupTask &task) const;
    QJsonObject queuedTaskToJson(const QueuedBackupTask &task) const;
    QueuedBackupTask queuedTaskFromJson(const QJsonObject &object) const;
    void handleBackupFinished(const BackupExecutionResult &result);
    void importArchivePath(const QString &archivePath);
    BackupRequest buildRequest() const;

    ProjectService *m_projectService = nullptr;
    MaterialBackupService *m_backupService = nullptr;
    ImportService *m_importService = nullptr;
    AppSettings *m_settings = nullptr;
    BackupSourceListModel *m_sourceModel = nullptr;
    BackupDestinationListModel *m_destinationModel = nullptr;
    BackupTaskListModel *m_taskModel = nullptr;
    QVector<BackupSource> m_sources;
    QVector<BackupDestination> m_destinations;
    QVector<QueuedBackupTask> m_queuedTasks;
    QVector<BackupDestinationTask> m_historyTasks;
    QVector<BackupDestinationTask> m_activeDestinationTasks;
    QueuedBackupTask m_activeQueuedTask;
    BackupPlan m_activePlan;
    BackupPlan m_lastPlan;
    QString m_lastMessage;
    bool m_running = false;
    bool m_hasActiveQueuedTask = false;
    bool m_cancelQueueRequested = false;
    bool m_cascadeEnabled = false;
    int m_verificationMode = 0;
    int m_primaryDestinationIndex = 0;
    int m_activeHistoryIndex = -1;
    int m_overallProgress = 0;
};

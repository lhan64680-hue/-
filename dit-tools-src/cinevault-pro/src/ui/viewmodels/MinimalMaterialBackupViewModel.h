#pragma once

#include <QObject>
#include <QVariantList>

class BackupDestinationListModel;
class BackupSourceListModel;
class BackupTaskListModel;

class MinimalMaterialBackupViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString summaryText READ summaryText CONSTANT)
    Q_PROPERTY(bool hasOpenProject READ hasOpenProject CONSTANT)
    Q_PROPERTY(QString projectPath READ projectPath CONSTANT)
    Q_PROPERTY(QString lastMessage READ lastMessage CONSTANT)
    Q_PROPERTY(bool running READ running CONSTANT)
    Q_PROPERTY(bool canStartBackup READ canStartBackup CONSTANT)
    Q_PROPERTY(bool canAddBackupTask READ canAddBackupTask CONSTANT)
    Q_PROPERTY(int queuedTaskCount READ queuedTaskCount CONSTANT)
    Q_PROPERTY(bool cascadeEnabled READ cascadeEnabled WRITE setCascadeEnabled NOTIFY stateChanged)
    Q_PROPERTY(int verificationMode READ verificationMode WRITE setVerificationMode NOTIFY stateChanged)
    Q_PROPERTY(QVariantList verificationOptions READ verificationOptions CONSTANT)
    Q_PROPERTY(int overallProgress READ overallProgress CONSTANT)
    Q_PROPERTY(QObject *sourceModel READ sourceModel CONSTANT)
    Q_PROPERTY(QObject *destinationModel READ destinationModel CONSTANT)
    Q_PROPERTY(QObject *taskModel READ taskModel CONSTANT)

public:
    explicit MinimalMaterialBackupViewModel(QObject *parent = nullptr);

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
    BackupSourceListModel *m_sourceModel = nullptr;
    BackupDestinationListModel *m_destinationModel = nullptr;
    BackupTaskListModel *m_taskModel = nullptr;
    bool m_cascadeEnabled = false;
    int m_verificationMode = 0;
};

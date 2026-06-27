#include "ui/viewmodels/MinimalMaterialBackupViewModel.h"

#include "ui/models/BackupDestinationListModel.h"
#include "ui/models/BackupSourceListModel.h"
#include "ui/models/BackupTaskListModel.h"

MinimalMaterialBackupViewModel::MinimalMaterialBackupViewModel(QObject *parent)
    : QObject(parent)
    , m_sourceModel(new BackupSourceListModel(this))
    , m_destinationModel(new BackupDestinationListModel(this))
    , m_taskModel(new BackupTaskListModel(this))
{
}

QString MinimalMaterialBackupViewModel::summaryText() const
{
    return QStringLiteral("素材备份页面骨架可测试");
}

bool MinimalMaterialBackupViewModel::hasOpenProject() const
{
    return true;
}

QString MinimalMaterialBackupViewModel::projectPath() const
{
    return QStringLiteral("最小 GUI 测试项目");
}

QString MinimalMaterialBackupViewModel::lastMessage() const
{
    return QStringLiteral("当前为最小 GUI 占位模式。");
}

bool MinimalMaterialBackupViewModel::running() const
{
    return false;
}

bool MinimalMaterialBackupViewModel::canStartBackup() const
{
    return false;
}

bool MinimalMaterialBackupViewModel::canAddBackupTask() const
{
    return false;
}

int MinimalMaterialBackupViewModel::queuedTaskCount() const
{
    return 0;
}

bool MinimalMaterialBackupViewModel::cascadeEnabled() const
{
    return m_cascadeEnabled;
}

int MinimalMaterialBackupViewModel::verificationMode() const
{
    return m_verificationMode;
}

QVariantList MinimalMaterialBackupViewModel::verificationOptions() const
{
    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral("不校验")}, {QStringLiteral("value"), 0}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("大小校验")}, {QStringLiteral("value"), 1}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("SHA-256")}, {QStringLiteral("value"), 2}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("MD5")}, {QStringLiteral("value"), 3}}
    };
}

int MinimalMaterialBackupViewModel::overallProgress() const
{
    return 0;
}

QObject *MinimalMaterialBackupViewModel::sourceModel() const
{
    return m_sourceModel;
}

QObject *MinimalMaterialBackupViewModel::destinationModel() const
{
    return m_destinationModel;
}

QObject *MinimalMaterialBackupViewModel::taskModel() const
{
    return m_taskModel;
}

void MinimalMaterialBackupViewModel::setCascadeEnabled(bool enabled)
{
    if (m_cascadeEnabled == enabled) {
        return;
    }
    m_cascadeEnabled = enabled;
    emit stateChanged();
}

void MinimalMaterialBackupViewModel::setVerificationMode(int mode)
{
    if (m_verificationMode == mode) {
        return;
    }
    m_verificationMode = mode;
    emit stateChanged();
}

void MinimalMaterialBackupViewModel::addFileSources()
{
}

void MinimalMaterialBackupViewModel::addFolderSource()
{
}

void MinimalMaterialBackupViewModel::addVolumeSource()
{
}

void MinimalMaterialBackupViewModel::addDestination()
{
}

void MinimalMaterialBackupViewModel::removeSource(int)
{
}

void MinimalMaterialBackupViewModel::removeDestination(int)
{
}

void MinimalMaterialBackupViewModel::setPrimaryDestination(int)
{
}

void MinimalMaterialBackupViewModel::enqueueBackupTask()
{
}

void MinimalMaterialBackupViewModel::removeQueuedTask(const QString &)
{
}

void MinimalMaterialBackupViewModel::startBackup()
{
}

void MinimalMaterialBackupViewModel::cancelBackup()
{
}

#include "ui/viewmodels/MaterialBackupViewModel.h"

#include "application/ImportService.h"
#include "application/MaterialBackupService.h"
#include "application/ProjectService.h"
#include "infrastructure/config/AppSettings.h"
#include "shared/Formatters.h"
#include "ui/models/BackupDestinationListModel.h"
#include "ui/models/BackupSourceListModel.h"
#include "ui/models/BackupTaskListModel.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSet>
#include <QStorageInfo>
#include <QUuid>

namespace {
QWidget *dialogParent()
{
    return QApplication::activeWindow();
}

QString absolutePath(const QString &path)
{
    return QFileInfo(path).absoluteFilePath();
}

bool containsPath(const QVector<BackupSource> &sources, const QString &path)
{
    const auto normalized = absolutePath(path);
    for (const auto &source : sources) {
        if (absolutePath(source.path).compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool containsDestination(const QVector<BackupDestination> &destinations, const QString &path)
{
    const auto normalized = absolutePath(path);
    for (const auto &destination : destinations) {
        if (absolutePath(destination.rootPath).compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

BackupSourceKind sourceKindFromInt(int value)
{
    switch (static_cast<BackupSourceKind>(value)) {
    case BackupSourceKind::File: return BackupSourceKind::File;
    case BackupSourceKind::Volume: return BackupSourceKind::Volume;
    case BackupSourceKind::Directory:
    default: return BackupSourceKind::Directory;
    }
}

BackupVerificationMode verificationModeFromInt(int value)
{
    switch (static_cast<BackupVerificationMode>(value)) {
    case BackupVerificationMode::Size: return BackupVerificationMode::Size;
    case BackupVerificationMode::Sha256: return BackupVerificationMode::Sha256;
    case BackupVerificationMode::Md5: return BackupVerificationMode::Md5;
    case BackupVerificationMode::Off:
    default: return BackupVerificationMode::Off;
    }
}

QJsonObject sourceToJson(const BackupSource &source)
{
    QJsonObject object;
    object.insert(QStringLiteral("kind"), static_cast<int>(source.kind));
    object.insert(QStringLiteral("name"), source.name);
    object.insert(QStringLiteral("path"), source.path);
    return object;
}

BackupSource sourceFromJson(const QJsonObject &object)
{
    BackupSource source;
    source.kind = sourceKindFromInt(object.value(QStringLiteral("kind")).toInt(static_cast<int>(BackupSourceKind::Directory)));
    source.name = object.value(QStringLiteral("name")).toString();
    source.path = object.value(QStringLiteral("path")).toString();
    return source;
}

QJsonObject destinationToJson(const BackupDestination &destination)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), destination.name);
    object.insert(QStringLiteral("rootPath"), destination.rootPath);
    object.insert(QStringLiteral("primary"), destination.primary);
    return object;
}

BackupDestination destinationFromJson(const QJsonObject &object)
{
    BackupDestination destination;
    destination.name = object.value(QStringLiteral("name")).toString();
    destination.rootPath = object.value(QStringLiteral("rootPath")).toString();
    destination.primary = object.value(QStringLiteral("primary")).toBool(false);
    return destination;
}

QString volumeLabel(const QStorageInfo &storage)
{
    const auto rootPath = storage.rootPath();
    const auto displayName = storage.displayName().trimmed();
    const auto name = storage.name().trimmed();
    QString label = displayName.isEmpty() ? rootPath : displayName;
    if (!name.isEmpty() && name != displayName) {
        label += QStringLiteral(" / %1").arg(name);
    }
    return QStringLiteral("%1 (%2，可用 %3)")
        .arg(label, rootPath, Formatters::formatBytes(storage.bytesAvailable()));
}
}

MaterialBackupViewModel::MaterialBackupViewModel(ProjectService *projectService,
                                                 MaterialBackupService *backupService,
                                                 ImportService *importService,
                                                 AppSettings *settings,
                                                 QObject *parent)
    : QObject(parent)
    , m_projectService(projectService)
    , m_backupService(backupService)
    , m_importService(importService)
    , m_settings(settings)
    , m_sourceModel(new BackupSourceListModel(this))
    , m_destinationModel(new BackupDestinationListModel(this))
    , m_taskModel(new BackupTaskListModel(this))
{
    connect(m_projectService, &ProjectService::projectChanged, this, &MaterialBackupViewModel::resetForProject);
    connect(m_backupService, &MaterialBackupService::backupStarted, this, [this]() {
        m_running = true;
        m_overallProgress = 0;
        emit stateChanged();
    });
    connect(m_backupService, &MaterialBackupService::backupTaskUpdated, this, [this](const BackupDestinationTask &task) {
        updateActiveDestinationTask(task);
        recomputeOverallProgress();
        emit stateChanged();
    });
    connect(m_backupService, &MaterialBackupService::messageChanged, this, [this](const QString &message) {
        m_lastMessage = message;
        emit stateChanged();
    });
    connect(m_backupService, &MaterialBackupService::backupFinished, this, [this](const BackupExecutionResult &result) {
        if (!result.warnings.isEmpty()) {
            m_lastMessage = result.warnings.join(QStringLiteral("\n"));
        }
        if (!result.errors.isEmpty()) {
            m_lastMessage = result.errors.join(QStringLiteral("\n"));
        }
        finishActiveQueuedTask(result);
    });

    resetForProject();
}

QString MaterialBackupViewModel::summaryText() const
{
    if (!hasOpenProject()) {
        return QStringLiteral("请先创建或打开项目");
    }
    if (m_running) {
        return QStringLiteral("素材备份队列执行中，剩余 %1 个任务").arg(m_queuedTasks.size());
    }
    if (!m_queuedTasks.isEmpty()) {
        return QStringLiteral("已添加 %1 个等待任务，点击开始备份").arg(m_queuedTasks.size());
    }
    if (!m_lastPlan.errors.isEmpty()) {
        return m_lastPlan.errors.first();
    }
    if (m_sources.isEmpty() || m_destinations.isEmpty()) {
        return QStringLiteral("添加源和目的地后先加入备份任务");
    }
    return QStringLiteral("当前计划 %1 个文件，%2 个目的地，可加入备份任务").arg(m_lastPlan.totalFiles).arg(m_destinations.size());
}

bool MaterialBackupViewModel::hasOpenProject() const
{
    return m_projectService && m_projectService->hasOpenProject();
}

QString MaterialBackupViewModel::projectPath() const
{
    return hasOpenProject() ? m_projectService->currentProject().rootPath : QString();
}

QString MaterialBackupViewModel::lastMessage() const
{
    return m_lastMessage;
}

bool MaterialBackupViewModel::running() const
{
    return m_running;
}

bool MaterialBackupViewModel::canStartBackup() const
{
    return hasOpenProject() && !m_running && !m_queuedTasks.isEmpty();
}

bool MaterialBackupViewModel::canAddBackupTask() const
{
    return hasOpenProject() && !m_running && !m_sources.isEmpty() && !m_destinations.isEmpty() && m_lastPlan.valid;
}

int MaterialBackupViewModel::queuedTaskCount() const
{
    return m_queuedTasks.size();
}

bool MaterialBackupViewModel::cascadeEnabled() const
{
    return m_cascadeEnabled;
}

int MaterialBackupViewModel::verificationMode() const
{
    return m_verificationMode;
}

QVariantList MaterialBackupViewModel::verificationOptions() const
{
    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral("不校验")}, {QStringLiteral("value"), static_cast<int>(BackupVerificationMode::Off)}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("大小校验")}, {QStringLiteral("value"), static_cast<int>(BackupVerificationMode::Size)}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("SHA-256")}, {QStringLiteral("value"), static_cast<int>(BackupVerificationMode::Sha256)}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("MD5")}, {QStringLiteral("value"), static_cast<int>(BackupVerificationMode::Md5)}}
    };
}

int MaterialBackupViewModel::overallProgress() const
{
    return m_overallProgress;
}

QObject *MaterialBackupViewModel::sourceModel() const
{
    return m_sourceModel;
}

QObject *MaterialBackupViewModel::destinationModel() const
{
    return m_destinationModel;
}

QObject *MaterialBackupViewModel::taskModel() const
{
    return m_taskModel;
}

void MaterialBackupViewModel::setCascadeEnabled(bool enabled)
{
    if (m_cascadeEnabled == enabled) {
        return;
    }
    m_cascadeEnabled = enabled;
    refreshPlan();
}

void MaterialBackupViewModel::setVerificationMode(int mode)
{
    if (m_verificationMode == mode) {
        return;
    }
    m_verificationMode = mode;
    refreshPlan();
}

void MaterialBackupViewModel::addFileSources()
{
    if (m_running) {
        return;
    }
    const auto files = QFileDialog::getOpenFileNames(dialogParent(), QStringLiteral("选择待备份文件"), projectPath());
    for (const auto &file : files) {
        addSourcePath(file, BackupSourceKind::File);
    }
    refreshPlan();
}

void MaterialBackupViewModel::addFolderSource()
{
    if (m_running) {
        return;
    }
    const auto folder = QFileDialog::getExistingDirectory(dialogParent(), QStringLiteral("选择待备份文件夹"), projectPath());
    addSourcePath(folder, BackupSourceKind::Directory);
    refreshPlan();
}

void MaterialBackupViewModel::addVolumeSource()
{
    if (m_running) {
        return;
    }

    QStringList labels;
    QStringList paths;
    QSet<QString> seenRoots;
    const auto volumes = QStorageInfo::mountedVolumes();
    for (const auto &volume : volumes) {
        const auto rootPath = volume.rootPath();
        if (!volume.isValid() || !volume.isReady() || rootPath.trimmed().isEmpty() || seenRoots.contains(rootPath)) {
            continue;
        }
        seenRoots.insert(rootPath);
        labels.append(volumeLabel(volume));
        paths.append(rootPath);
    }

    if (paths.isEmpty()) {
        m_lastMessage = QStringLiteral("未发现可用的磁盘分区。");
        QMessageBox::information(dialogParent(), QStringLiteral("添加磁盘卷"), m_lastMessage);
        emit stateChanged();
        return;
    }

    bool ok = false;
    const auto selectedLabel = QInputDialog::getItem(dialogParent(),
                                                     QStringLiteral("选择待备份磁盘卷"),
                                                     QStringLiteral("选择一个已挂载的磁盘分区："),
                                                     labels,
                                                     0,
                                                     false,
                                                     &ok);
    const auto selectedIndex = labels.indexOf(selectedLabel);
    if (!ok || selectedIndex < 0 || selectedIndex >= paths.size()) {
        refreshPlan();
        return;
    }

    addSourcePath(paths.at(selectedIndex), BackupSourceKind::Volume);
    refreshPlan();
}

void MaterialBackupViewModel::addDestination()
{
    if (m_running) {
        return;
    }
    const auto folder = QFileDialog::getExistingDirectory(dialogParent(), QStringLiteral("选择备份目的地"), projectPath());
    if (folder.isEmpty() || containsDestination(m_destinations, folder)) {
        refreshPlan();
        return;
    }

    BackupDestination destination;
    destination.rootPath = folder;
    destination.name = QFileInfo(folder).fileName().isEmpty() ? folder : QFileInfo(folder).fileName();
    destination.primary = m_destinations.isEmpty();
    m_destinations.append(destination);
    if (destination.primary) {
        m_primaryDestinationIndex = 0;
    }
    refreshPlan();
}

void MaterialBackupViewModel::removeSource(int row)
{
    if (m_running || row < 0 || row >= m_sources.size()) {
        return;
    }
    m_sources.removeAt(row);
    refreshPlan();
}

void MaterialBackupViewModel::removeDestination(int row)
{
    if (m_running || row < 0 || row >= m_destinations.size()) {
        return;
    }
    m_destinations.removeAt(row);
    m_primaryDestinationIndex = qBound(0, m_primaryDestinationIndex, qMax(0, m_destinations.size() - 1));
    if (!m_destinations.isEmpty()) {
        for (int i = 0; i < m_destinations.size(); ++i) {
            m_destinations[i].primary = i == m_primaryDestinationIndex;
        }
    }
    refreshPlan();
}

void MaterialBackupViewModel::setPrimaryDestination(int row)
{
    if (m_running || row < 0 || row >= m_destinations.size()) {
        return;
    }
    m_primaryDestinationIndex = row;
    for (int i = 0; i < m_destinations.size(); ++i) {
        m_destinations[i].primary = i == row;
    }
    refreshPlan();
}

void MaterialBackupViewModel::enqueueBackupTask()
{
    if (m_running) {
        return;
    }
    refreshPlan();
    if (!canAddBackupTask()) {
        m_lastMessage = m_lastPlan.errors.isEmpty()
            ? QStringLiteral("请先添加可用源、目的地并打开项目。")
            : m_lastPlan.errors.join(QStringLiteral("\n"));
        QMessageBox::warning(dialogParent(), QStringLiteral("无法添加备份任务"), m_lastMessage);
        emit stateChanged();
        return;
    }

    const auto now = QDateTime::currentDateTime();
    BackupRequest request = buildRequest();
    request.batchName = QStringLiteral("%1_%2").arg(m_lastPlan.batchName, now.toString(QStringLiteral("zzz")));
    auto plan = m_backupService->buildPlan(request);
    if (!plan.valid) {
        m_lastMessage = plan.errors.join(QStringLiteral("\n"));
        QMessageBox::warning(dialogParent(), QStringLiteral("无法添加备份任务"), m_lastMessage);
        emit stateChanged();
        return;
    }

    QueuedBackupTask task;
    task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    task.batchName = plan.batchName;
    task.createdAt = now.toString(Qt::ISODate);
    task.sources = plan.sources;
    task.destinations = plan.destinations;
    task.verificationMode = plan.verificationMode;
    task.cascadeEnabled = plan.cascadeEnabled;
    task.primaryDestinationIndex = plan.primaryDestinationIndex;
    task.totalFiles = plan.totalFiles;
    task.totalBytes = plan.totalBytes;
    task.plannedRootPath = plan.destinations.isEmpty() ? QString() : plan.destinations.first().plannedRootPath;
    m_queuedTasks.append(task);

    persistQueue();
    clearDraft();
    m_lastMessage = QStringLiteral("已添加到备份任务：%1").arg(task.batchName);
    refreshPlan();
}

void MaterialBackupViewModel::removeQueuedTask(const QString &taskId)
{
    if (m_running || taskId.trimmed().isEmpty()) {
        return;
    }
    for (int i = 0; i < m_queuedTasks.size(); ++i) {
        if (m_queuedTasks.at(i).id != taskId) {
            continue;
        }
        m_queuedTasks.removeAt(i);
        persistQueue();
        rebuildTaskModel();
        m_lastMessage = QStringLiteral("已移除等待任务。");
        emit stateChanged();
        return;
    }
}

void MaterialBackupViewModel::startBackup()
{
    if (!canStartBackup()) {
        m_lastMessage = hasOpenProject()
            ? QStringLiteral("请先点击“添加到备份任务”，再开始备份。")
            : QStringLiteral("请先创建或打开项目。");
        QMessageBox::warning(dialogParent(), QStringLiteral("无法开始素材备份"), m_lastMessage);
        emit stateChanged();
        return;
    }

    m_historyTasks.clear();
    m_activeDestinationTasks.clear();
    m_activeHistoryIndex = -1;
    m_cancelQueueRequested = false;
    m_overallProgress = 0;
    rebuildTaskModel();
    startNextQueuedTask();
}

void MaterialBackupViewModel::cancelBackup()
{
    if (!m_running) {
        return;
    }
    m_cancelQueueRequested = true;
    m_backupService->cancelBackup();
}

void MaterialBackupViewModel::resetForProject()
{
    if (m_running) {
        return;
    }
    m_sources.clear();
    m_destinations.clear();
    m_historyTasks.clear();
    m_activeDestinationTasks.clear();
    m_hasActiveQueuedTask = false;
    m_activeHistoryIndex = -1;
    m_primaryDestinationIndex = 0;
    m_overallProgress = 0;
    loadQueueForProject();
    refreshPlan();
}

void MaterialBackupViewModel::loadQueueForProject()
{
    m_queuedTasks.clear();
    if (!m_settings || !hasOpenProject()) {
        rebuildTaskModel();
        return;
    }

    const auto json = m_settings->materialBackupQueueJson(m_projectService->currentProject().databasePath);
    const auto document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        rebuildTaskModel();
        return;
    }

    const auto array = document.array();
    for (const auto &value : array) {
        if (!value.isObject()) {
            continue;
        }
        auto task = queuedTaskFromJson(value.toObject());
        if (!task.sources.isEmpty() && !task.destinations.isEmpty()) {
            m_queuedTasks.append(task);
        }
    }
    rebuildTaskModel();
}

void MaterialBackupViewModel::persistQueue() const
{
    if (!m_settings || !hasOpenProject()) {
        return;
    }

    QJsonArray array;
    for (const auto &task : m_queuedTasks) {
        array.append(queuedTaskToJson(task));
    }
    const auto json = QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
    m_settings->setMaterialBackupQueueJson(m_projectService->currentProject().databasePath, json);
}

void MaterialBackupViewModel::rebuildTaskModel()
{
    QVector<BackupDestinationTask> rows = m_historyTasks;
    rows.reserve(m_historyTasks.size() + m_queuedTasks.size());
    for (const auto &task : m_queuedTasks) {
        rows.append(taskRowForQueuedTask(task));
    }
    m_taskModel->setItems(rows);
}

void MaterialBackupViewModel::clearDraft()
{
    m_sources.clear();
    m_destinations.clear();
    m_primaryDestinationIndex = 0;
    m_sourceModel->setItems(m_sources);
    m_destinationModel->setItems(m_destinations);
}

void MaterialBackupViewModel::startNextQueuedTask()
{
    if (m_running || !hasOpenProject()) {
        rebuildTaskModel();
        emit stateChanged();
        return;
    }

    while (!m_queuedTasks.isEmpty()) {
        m_activeQueuedTask = m_queuedTasks.takeFirst();
        m_hasActiveQueuedTask = true;
        m_activeDestinationTasks.clear();
        m_activePlan = m_backupService->buildPlan(requestForQueuedTask(m_activeQueuedTask));
        persistQueue();

        m_activeHistoryIndex = m_historyTasks.size();
        m_historyTasks.append(activeTaskRow(BackupTaskState::Pending, QStringLiteral("准备开始"), QString()));
        rebuildTaskModel();

        if (!m_activePlan.valid) {
            const auto errorMessage = m_activePlan.errors.join(QStringLiteral("\n"));
            m_historyTasks[m_activeHistoryIndex] = activeTaskRow(BackupTaskState::Failed, QStringLiteral("任务无效"), errorMessage);
            m_lastMessage = QStringLiteral("备份任务无效，已跳过：%1").arg(errorMessage);
            m_hasActiveQueuedTask = false;
            m_activeHistoryIndex = -1;
            rebuildTaskModel();
            continue;
        }

        m_activeDestinationTasks = m_activePlan.tasks;
        m_historyTasks[m_activeHistoryIndex] = activeTaskRow(BackupTaskState::Running, QStringLiteral("正在开始备份"), QString());
        rebuildTaskModel();
        if (!m_backupService->startBackup(m_activePlan)) {
            m_historyTasks[m_activeHistoryIndex] = activeTaskRow(BackupTaskState::Failed,
                                                                 QStringLiteral("无法启动"),
                                                                 QStringLiteral("素材备份任务正在运行。"));
            m_lastMessage = QStringLiteral("素材备份任务正在运行。");
            m_hasActiveQueuedTask = false;
            m_activeHistoryIndex = -1;
            rebuildTaskModel();
            continue;
        }
        return;
    }

    m_running = false;
    m_hasActiveQueuedTask = false;
    m_activeDestinationTasks.clear();
    m_activeHistoryIndex = -1;
    m_overallProgress = 0;
    rebuildTaskModel();
    emit stateChanged();
}

void MaterialBackupViewModel::finishActiveQueuedTask(const BackupExecutionResult &result)
{
    const auto wasCancelled = result.cancelled || m_cancelQueueRequested;
    const auto state = result.success
        ? BackupTaskState::Completed
        : (wasCancelled ? BackupTaskState::Cancelled : BackupTaskState::Failed);
    const auto statusText = result.success
        ? QStringLiteral("备份完成")
        : (wasCancelled ? QStringLiteral("备份已取消") : QStringLiteral("备份失败"));
    const auto errorMessage = result.errors.isEmpty()
        ? result.warnings.join(QStringLiteral("\n"))
        : result.errors.join(QStringLiteral("\n"));

    m_running = false;
    m_overallProgress = result.success ? 100 : m_overallProgress;
    if (m_hasActiveQueuedTask && m_activeHistoryIndex >= 0 && m_activeHistoryIndex < m_historyTasks.size()) {
        m_historyTasks[m_activeHistoryIndex] = activeTaskRow(state, statusText, errorMessage);
    }

    handleBackupFinished(result);

    m_hasActiveQueuedTask = false;
    m_activeDestinationTasks.clear();
    m_activeHistoryIndex = -1;
    rebuildTaskModel();

    if (!wasCancelled && !m_queuedTasks.isEmpty()) {
        startNextQueuedTask();
        return;
    }

    emit stateChanged();
}

void MaterialBackupViewModel::updateActiveDestinationTask(const BackupDestinationTask &task)
{
    bool updated = false;
    for (int i = 0; i < m_activeDestinationTasks.size(); ++i) {
        if (m_activeDestinationTasks.at(i).destinationId != task.destinationId) {
            continue;
        }
        m_activeDestinationTasks[i] = task;
        updated = true;
        break;
    }
    if (!updated) {
        m_activeDestinationTasks.append(task);
    }

    if (m_hasActiveQueuedTask && m_activeHistoryIndex >= 0 && m_activeHistoryIndex < m_historyTasks.size()) {
        const auto aggregateState = task.state == BackupTaskState::Verifying
            ? BackupTaskState::Verifying
            : BackupTaskState::Running;
        m_historyTasks[m_activeHistoryIndex] = activeTaskRow(aggregateState, task.statusText, task.errorMessage);
        rebuildTaskModel();
    }
}

BackupDestinationTask MaterialBackupViewModel::taskRowForQueuedTask(const QueuedBackupTask &task) const
{
    BackupDestinationTask row;
    row.destinationId = task.id;
    row.name = task.batchName;
    row.plannedRootPath = task.plannedRootPath.isEmpty()
        ? QStringLiteral("%1 个源 -> %2 个目的地").arg(task.sources.size()).arg(task.destinations.size())
        : task.plannedRootPath;
    row.state = BackupTaskState::Pending;
    const auto destinationCount = qMax<qint64>(1, static_cast<qint64>(task.destinations.size()));
    row.totalFiles = task.totalFiles * destinationCount;
    row.totalBytes = task.totalBytes * destinationCount;
    row.statusText = QStringLiteral("等待执行 · %1 个源 · %2 个目的地").arg(task.sources.size()).arg(task.destinations.size());
    return row;
}

BackupDestinationTask MaterialBackupViewModel::activeTaskRow(BackupTaskState fallbackState,
                                                             const QString &statusText,
                                                             const QString &errorMessage) const
{
    BackupDestinationTask row;
    row.destinationId = m_activeQueuedTask.id;
    row.name = m_activeQueuedTask.batchName;
    row.plannedRootPath = m_activePlan.destinations.size() == 1
        ? m_activePlan.destinations.first().plannedRootPath
        : QStringLiteral("%1 个目的地 · %2").arg(m_activePlan.destinations.size()).arg(m_activeQueuedTask.plannedRootPath);
    row.primary = true;
    row.state = fallbackState;
    row.statusText = statusText;
    row.errorMessage = errorMessage;

    const auto tasks = m_activeDestinationTasks.isEmpty() ? m_activePlan.tasks : m_activeDestinationTasks;
    for (const auto &task : tasks) {
        row.totalFiles += task.totalFiles;
        row.copiedFiles += task.copiedFiles;
        row.totalBytes += task.totalBytes;
        row.copiedBytes += task.copiedBytes;
        row.bytesPerSecond += task.bytesPerSecond;
        if (fallbackState == BackupTaskState::Running || fallbackState == BackupTaskState::Verifying) {
            if (task.state == BackupTaskState::Failed || task.state == BackupTaskState::Cancelled || task.state == BackupTaskState::Verifying) {
                row.state = task.state;
            }
        }
    }
    if (fallbackState == BackupTaskState::Completed) {
        row.copiedFiles = row.totalFiles;
        row.copiedBytes = row.totalBytes;
    }
    return row;
}

BackupRequest MaterialBackupViewModel::requestForQueuedTask(const QueuedBackupTask &task) const
{
    BackupRequest request;
    request.projectName = hasOpenProject() ? m_projectService->currentProject().name : QStringLiteral("未打开项目");
    request.batchName = task.batchName;
    request.sources = task.sources;
    request.destinations = task.destinations;
    request.verificationMode = task.verificationMode;
    request.cascadeEnabled = task.cascadeEnabled;
    request.primaryDestinationIndex = task.primaryDestinationIndex;
    for (int i = 0; i < request.destinations.size(); ++i) {
        request.destinations[i].primary = i == request.primaryDestinationIndex;
    }
    return request;
}

QJsonObject MaterialBackupViewModel::queuedTaskToJson(const QueuedBackupTask &task) const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), task.id);
    object.insert(QStringLiteral("batchName"), task.batchName);
    object.insert(QStringLiteral("createdAt"), task.createdAt);
    object.insert(QStringLiteral("verificationMode"), static_cast<int>(task.verificationMode));
    object.insert(QStringLiteral("cascadeEnabled"), task.cascadeEnabled);
    object.insert(QStringLiteral("primaryDestinationIndex"), task.primaryDestinationIndex);
    object.insert(QStringLiteral("totalFiles"), QString::number(task.totalFiles));
    object.insert(QStringLiteral("totalBytes"), QString::number(task.totalBytes));
    object.insert(QStringLiteral("plannedRootPath"), task.plannedRootPath);

    QJsonArray sources;
    for (const auto &source : task.sources) {
        sources.append(sourceToJson(source));
    }
    object.insert(QStringLiteral("sources"), sources);

    QJsonArray destinations;
    for (const auto &destination : task.destinations) {
        destinations.append(destinationToJson(destination));
    }
    object.insert(QStringLiteral("destinations"), destinations);
    return object;
}

MaterialBackupViewModel::QueuedBackupTask MaterialBackupViewModel::queuedTaskFromJson(const QJsonObject &object) const
{
    QueuedBackupTask task;
    task.id = object.value(QStringLiteral("id")).toString();
    if (task.id.trimmed().isEmpty()) {
        task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    task.batchName = object.value(QStringLiteral("batchName")).toString();
    task.createdAt = object.value(QStringLiteral("createdAt")).toString();
    task.verificationMode = verificationModeFromInt(object.value(QStringLiteral("verificationMode")).toInt());
    task.cascadeEnabled = object.value(QStringLiteral("cascadeEnabled")).toBool(false);
    task.primaryDestinationIndex = object.value(QStringLiteral("primaryDestinationIndex")).toInt(0);
    task.totalFiles = object.value(QStringLiteral("totalFiles")).toString().toLongLong();
    task.totalBytes = object.value(QStringLiteral("totalBytes")).toString().toLongLong();
    task.plannedRootPath = object.value(QStringLiteral("plannedRootPath")).toString();

    const auto sources = object.value(QStringLiteral("sources")).toArray();
    for (const auto &value : sources) {
        if (!value.isObject()) {
            continue;
        }
        auto source = sourceFromJson(value.toObject());
        if (!source.path.trimmed().isEmpty()) {
            task.sources.append(source);
        }
    }

    const auto destinations = object.value(QStringLiteral("destinations")).toArray();
    for (const auto &value : destinations) {
        if (!value.isObject()) {
            continue;
        }
        auto destination = destinationFromJson(value.toObject());
        if (!destination.rootPath.trimmed().isEmpty()) {
            task.destinations.append(destination);
        }
    }

    task.primaryDestinationIndex = qBound(0, task.primaryDestinationIndex, qMax(0, task.destinations.size() - 1));
    for (int i = 0; i < task.destinations.size(); ++i) {
        task.destinations[i].primary = i == task.primaryDestinationIndex;
    }
    if (task.batchName.trimmed().isEmpty()) {
        task.batchName = QStringLiteral("backup_queued");
    }
    return task;
}

void MaterialBackupViewModel::refreshPlan()
{
    if (!m_backupService) {
        return;
    }
    m_lastPlan = m_backupService->buildPlan(buildRequest());
    if (m_lastPlan.sources.size() == m_sources.size()) {
        m_sources = m_lastPlan.sources;
    }
    if (m_lastPlan.destinations.size() == m_destinations.size()) {
        m_destinations = m_lastPlan.destinations;
    }
    m_sourceModel->setItems(m_sources);
    m_destinationModel->setItems(m_destinations);
    rebuildTaskModel();
    emit stateChanged();
}

void MaterialBackupViewModel::recomputeOverallProgress()
{
    const auto tasks = m_activeDestinationTasks;
    qint64 copied = 0;
    qint64 total = 0;
    for (const auto &task : tasks) {
        copied += task.copiedBytes;
        total += task.totalBytes;
    }
    m_overallProgress = total > 0 ? qBound(0, static_cast<int>((copied * qint64{100}) / total), 100) : 0;
}

void MaterialBackupViewModel::addSourcePath(const QString &path, BackupSourceKind kind)
{
    if (path.isEmpty() || containsPath(m_sources, path)) {
        return;
    }
    BackupSource source;
    source.kind = kind;
    source.path = path;
    source.name = QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName();
    m_sources.append(source);
}

void MaterialBackupViewModel::handleBackupFinished(const BackupExecutionResult &result)
{
    if (result.cancelled || result.successfulArchivePaths.isEmpty()) {
        return;
    }

    if (result.successfulArchivePaths.size() == 1) {
        importArchivePath(result.successfulArchivePaths.first());
        return;
    }

    QStringList choices;
    for (const auto &path : result.successfulArchivePaths) {
        choices.append(path);
    }

    bool ok = false;
    const auto selectedPath = QInputDialog::getItem(dialogParent(),
                                                    QStringLiteral("选择素材库归档路径"),
                                                    QStringLiteral("本次备份已写入多个位置，请选择素材库使用的路径："),
                                                    choices,
                                                    0,
                                                    false,
                                                    &ok);
    if (!ok || selectedPath.isEmpty()) {
        m_lastMessage = QStringLiteral("备份完成，已取消自动归档到素材库。");
        return;
    }
    importArchivePath(selectedPath);
}

void MaterialBackupViewModel::importArchivePath(const QString &archivePath)
{
    if (!m_importService || archivePath.trimmed().isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_importService->importDirectory(archivePath, &errorMessage)) {
        m_lastMessage = QStringLiteral("备份完成，但归档到素材库失败：%1").arg(errorMessage);
        QMessageBox::warning(dialogParent(), QStringLiteral("素材库归档失败"), m_lastMessage);
        return;
    }
    m_lastMessage = QStringLiteral("备份完成，已归档到素材库：%1").arg(archivePath);
}

BackupRequest MaterialBackupViewModel::buildRequest() const
{
    BackupRequest request;
    request.projectName = hasOpenProject() ? m_projectService->currentProject().name : QStringLiteral("未打开项目");
    request.sources = m_sources;
    request.destinations = m_destinations;
    request.verificationMode = static_cast<BackupVerificationMode>(m_verificationMode);
    request.cascadeEnabled = m_cascadeEnabled;
    request.primaryDestinationIndex = m_primaryDestinationIndex;
    return request;
}

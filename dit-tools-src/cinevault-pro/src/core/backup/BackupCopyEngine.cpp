#include "core/backup/BackupCopyEngine.h"

#include "core/backup/VolumeEjectService.h"

#include <QtConcurrent>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr qsizetype kCopyBufferSize = 4 * 1024 * 1024;

QString verificationModeName(BackupVerificationMode mode)
{
    switch (mode) {
    case BackupVerificationMode::Size: return QStringLiteral("size");
    case BackupVerificationMode::Sha256: return QStringLiteral("sha256");
    case BackupVerificationMode::Md5: return QStringLiteral("md5");
    case BackupVerificationMode::Off:
    default: return QStringLiteral("off");
    }
}

QCryptographicHash::Algorithm hashAlgorithm(BackupVerificationMode mode)
{
    return mode == BackupVerificationMode::Md5
        ? QCryptographicHash::Md5
        : QCryptographicHash::Sha256;
}

bool hashFile(const QString &path, BackupVerificationMode mode, QByteArray *digest, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取校验文件：%1").arg(path);
        }
        return false;
    }

    QCryptographicHash hash(hashAlgorithm(mode));
    QByteArray buffer;
    buffer.resize(kCopyBufferSize);
    while (!file.atEnd()) {
        const auto readSize = file.read(buffer.data(), buffer.size());
        if (readSize < 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("读取校验文件失败：%1").arg(path);
            }
            return false;
        }
        hash.addData(buffer.constData(), readSize);
    }
    if (digest) {
        *digest = hash.result();
    }
    return true;
}

bool verifyCopy(const BackupFileItem &file, const QString &targetPath, BackupVerificationMode mode, QString *errorMessage)
{
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标文件不存在：%1").arg(targetPath);
        }
        return false;
    }
    if (targetInfo.size() != file.sizeBytes) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("文件大小不一致：%1").arg(file.relativePath);
        }
        return false;
    }
    if (mode == BackupVerificationMode::Off || mode == BackupVerificationMode::Size) {
        return true;
    }

    QByteArray sourceDigest;
    QByteArray targetDigest;
    QString hashError;
    if (!hashFile(file.sourcePath, mode, &sourceDigest, &hashError)
        || !hashFile(targetPath, mode, &targetDigest, &hashError)) {
        if (errorMessage) {
            *errorMessage = hashError;
        }
        return false;
    }
    if (sourceDigest != targetDigest) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("哈希校验不一致：%1").arg(file.relativePath);
        }
        return false;
    }
    return true;
}

bool copyFileAtomically(const BackupFileItem &file,
                        const QString &targetPath,
                        BackupDestinationTask *task,
                        const BackupCopyEngine::ProgressCallback &progressCallback,
                        const std::atomic_bool &cancelRequested,
                        QString *errorMessage)
{
    QDir targetDir(QFileInfo(targetPath).absolutePath());
    if (!targetDir.exists() && !targetDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建目标目录：%1").arg(targetDir.absolutePath());
        }
        return false;
    }
    if (QFileInfo::exists(targetPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标文件已存在：%1").arg(targetPath);
        }
        return false;
    }

    QFile source(file.sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取源文件：%1").arg(file.sourcePath);
        }
        return false;
    }

    const auto partPath = targetPath + QStringLiteral(".cinevault-copying");
    if (QFileInfo::exists(partPath) && !QFile::remove(partPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法清理临时文件：%1").arg(partPath);
        }
        return false;
    }

    QFile target(partPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入目标文件：%1").arg(partPath);
        }
        return false;
    }

    QByteArray buffer;
    buffer.resize(kCopyBufferSize);
    while (!source.atEnd()) {
        if (cancelRequested.load()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("用户取消备份。");
            }
            target.close();
            QFile::remove(partPath);
            return false;
        }

        const auto readSize = source.read(buffer.data(), buffer.size());
        if (readSize < 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("读取源文件失败：%1").arg(file.sourcePath);
            }
            target.close();
            QFile::remove(partPath);
            return false;
        }
        if (target.write(buffer.constData(), readSize) != readSize) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("写入目标文件失败：%1").arg(targetPath);
            }
            target.close();
            QFile::remove(partPath);
            return false;
        }

        task->copiedBytes += readSize;
        if (progressCallback) {
            progressCallback(*task);
        }
    }

    if (!target.flush()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("刷新目标文件失败：%1").arg(targetPath);
        }
        target.close();
        QFile::remove(partPath);
        return false;
    }
    target.close();
    source.close();

    const auto modifiedTime = QDateTime::fromString(file.modifiedAt, Qt::ISODate);
    if (modifiedTime.isValid()) {
        QFile partFile(partPath);
        if (partFile.open(QIODevice::ReadWrite)) {
            partFile.setFileTime(modifiedTime, QFileDevice::FileModificationTime);
            partFile.close();
        }
    }

    if (!QFile::rename(partPath, targetPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法完成目标文件写入：%1").arg(targetPath);
        }
        QFile::remove(partPath);
        return false;
    }
    return true;
}

QString writeBackupLog(const BackupPlan &plan, const BackupDestinationTask &task, const QString &startedAt, const QString &finishedAt, const QStringList &errors)
{
    QJsonObject root;
    root.insert(QStringLiteral("application"), QStringLiteral("CineVault"));
    root.insert(QStringLiteral("kind"), QStringLiteral("material-backup"));
    root.insert(QStringLiteral("batchName"), plan.batchName);
    root.insert(QStringLiteral("destinationId"), task.destinationId);
    root.insert(QStringLiteral("destinationName"), task.name);
    root.insert(QStringLiteral("plannedRootPath"), task.plannedRootPath);
    root.insert(QStringLiteral("verificationMode"), verificationModeName(plan.verificationMode));
    root.insert(QStringLiteral("startedAt"), startedAt);
    root.insert(QStringLiteral("finishedAt"), finishedAt);
    root.insert(QStringLiteral("totalFiles"), task.totalFiles);
    root.insert(QStringLiteral("copiedFiles"), task.copiedFiles);
    root.insert(QStringLiteral("totalBytes"), QString::number(task.totalBytes));
    root.insert(QStringLiteral("copiedBytes"), QString::number(task.copiedBytes));
    root.insert(QStringLiteral("state"), static_cast<int>(task.state));

    QJsonArray sources;
    for (const auto &source : plan.sources) {
        QJsonObject row;
        row.insert(QStringLiteral("id"), source.id);
        row.insert(QStringLiteral("name"), source.name);
        row.insert(QStringLiteral("path"), source.path);
        row.insert(QStringLiteral("rootPath"), source.rootPath);
        row.insert(QStringLiteral("totalFiles"), source.totalFiles);
        row.insert(QStringLiteral("totalBytes"), QString::number(source.totalBytes));
        sources.append(row);
    }
    root.insert(QStringLiteral("sources"), sources);

    QJsonArray errorRows;
    for (const auto &error : errors) {
        errorRows.append(error);
    }
    root.insert(QStringLiteral("errors"), errorRows);

    QDir().mkpath(task.plannedRootPath);
    const auto logPath = QDir(task.plannedRootPath).filePath(QStringLiteral("CineVaultBackupLog.json"));
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    logFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return logPath;
}
}

BackupCopyEngine::BackupCopyEngine(QObject *parent)
    : QObject(parent)
{
}

BackupExecutionResult BackupCopyEngine::copyDirect(const BackupPlan &plan, const ProgressCallback &progressCallback)
{
    resetCancel();
    return copyTasksParallel(plan, plan.tasks, progressCallback);
}

BackupExecutionResult BackupCopyEngine::copyCascade(const BackupPlan &plan,
                                                    VolumeEjectService *volumeEjectService,
                                                    const ProgressCallback &progressCallback)
{
    resetCancel();

    if (!plan.cascadeEnabled || plan.tasks.size() <= 1) {
        return copyTasksParallel(plan, plan.tasks, progressCallback);
    }

    BackupDestinationTask primaryTask;
    QVector<BackupDestinationTask> secondaryTasks;
    bool hasPrimary = false;
    for (const auto &task : plan.tasks) {
        if (!hasPrimary && task.primary) {
            primaryTask = task;
            hasPrimary = true;
        } else {
            secondaryTasks.append(task);
        }
    }
    if (!hasPrimary && !plan.tasks.isEmpty()) {
        primaryTask = plan.tasks.first();
        primaryTask.primary = true;
        for (int i = 1; i < plan.tasks.size(); ++i) {
            secondaryTasks.append(plan.tasks.at(i));
        }
    }

    auto primaryResult = copyTasksParallel(plan, {primaryTask}, progressCallback);
    BackupExecutionResult result = primaryResult;
    if (primaryResult.cancelled || primaryResult.successfulArchivePaths.isEmpty()) {
        result.success = false;
        return result;
    }

    if (volumeEjectService) {
        for (const auto &source : plan.sources) {
            if (source.kind != BackupSourceKind::Volume) {
                continue;
            }
            QString ejectMessage;
            if (!volumeEjectService->ejectVolumeForPath(source.rootPath.isEmpty() ? source.path : source.rootPath, &ejectMessage)) {
                result.warnings.append(ejectMessage);
            } else if (!ejectMessage.isEmpty()) {
                result.warnings.append(ejectMessage);
            }
        }
    }

    if (secondaryTasks.isEmpty()) {
        result.success = true;
        return result;
    }

    BackupPlan cascadePlan = plan;
    cascadePlan.files.clear();
    const auto primaryRoot = primaryResult.successfulArchivePaths.first();
    for (auto file : plan.files) {
        file.sourcePath = QDir(primaryRoot).filePath(file.relativePath);
        cascadePlan.files.append(file);
    }
    cascadePlan.tasks = secondaryTasks;

    auto secondaryResult = copyTasksParallel(cascadePlan, secondaryTasks, progressCallback);
    result.tasks += secondaryResult.tasks;
    result.successfulArchivePaths += secondaryResult.successfulArchivePaths;
    result.logPaths += secondaryResult.logPaths;
    result.errors += secondaryResult.errors;
    result.warnings += secondaryResult.warnings;
    result.cancelled = result.cancelled || secondaryResult.cancelled;
    result.success = !result.successfulArchivePaths.isEmpty() && result.errors.isEmpty() && !result.cancelled;
    return result;
}

BackupExecutionResult BackupCopyEngine::copyTasksParallel(const BackupPlan &plan,
                                                          const QVector<BackupDestinationTask> &tasks,
                                                          const ProgressCallback &progressCallback) const
{
    BackupExecutionResult result;
    if (!plan.valid) {
        result.errors = plan.errors;
        result.success = false;
        return result;
    }

    QVector<QFuture<BackupDestinationTask>> futures;
    futures.reserve(tasks.size());
    for (const auto &task : tasks) {
        futures.append(QtConcurrent::run([this, plan, task, progressCallback]() {
            return copyDestination(plan, task, progressCallback);
        }));
    }

    for (auto &future : futures) {
        future.waitForFinished();
        auto task = future.result();
        result.tasks.append(task);
        if (task.state == BackupTaskState::Completed || task.state == BackupTaskState::Warning) {
            result.successfulArchivePaths.append(task.plannedRootPath);
            const auto logPath = QDir(task.plannedRootPath).filePath(QStringLiteral("CineVaultBackupLog.json"));
            if (QFileInfo::exists(logPath)) {
                result.logPaths.append(logPath);
            }
        } else if (task.state == BackupTaskState::Cancelled) {
            result.cancelled = true;
            result.errors.append(task.errorMessage);
        } else {
            result.errors.append(task.errorMessage);
        }
    }

    result.success = !result.successfulArchivePaths.isEmpty() && result.errors.isEmpty() && !result.cancelled;
    return result;
}

void BackupCopyEngine::requestCancel()
{
    m_cancelRequested = true;
}

void BackupCopyEngine::resetCancel()
{
    m_cancelRequested = false;
}

bool BackupCopyEngine::isCancelRequested() const
{
    return m_cancelRequested.load();
}

BackupDestinationTask BackupCopyEngine::copyDestination(const BackupPlan &plan,
                                                        BackupDestinationTask task,
                                                        const ProgressCallback &progressCallback) const
{
    const auto startedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    QStringList errors;
    QElapsedTimer speedTimer;
    speedTimer.start();

    task.state = BackupTaskState::Running;
    task.statusText = QStringLiteral("正在复制到 %1").arg(task.name);
    if (progressCallback) {
        progressCallback(task);
    }

    if (QFileInfo::exists(task.plannedRootPath)) {
        task.state = BackupTaskState::Failed;
        task.errorMessage = QStringLiteral("备份批次目录已存在：%1").arg(task.plannedRootPath);
        task.statusText = task.errorMessage;
        if (progressCallback) {
            progressCallback(task);
        }
        return task;
    }

    for (const auto &file : plan.files) {
        if (m_cancelRequested.load()) {
            task.state = BackupTaskState::Cancelled;
            task.errorMessage = QStringLiteral("用户取消备份。");
            task.statusText = task.errorMessage;
            if (progressCallback) {
                progressCallback(task);
            }
            return task;
        }

        const auto targetPath = QDir(task.plannedRootPath).filePath(file.relativePath);
        QString copyError;
        task.statusText = QStringLiteral("正在复制 %1").arg(file.relativePath);
        if (!copyFileAtomically(file, targetPath, &task, progressCallback, m_cancelRequested, &copyError)) {
            errors.append(copyError);
            task.state = m_cancelRequested.load() ? BackupTaskState::Cancelled : BackupTaskState::Failed;
            task.errorMessage = copyError;
            task.statusText = copyError;
            if (progressCallback) {
                progressCallback(task);
            }
            writeBackupLog(plan, task, startedAt, QDateTime::currentDateTime().toString(Qt::ISODate), errors);
            return task;
        }

        if (plan.verificationMode != BackupVerificationMode::Off) {
            task.state = BackupTaskState::Verifying;
            task.statusText = QStringLiteral("正在校验 %1").arg(file.relativePath);
            if (progressCallback) {
                progressCallback(task);
            }

            QString verifyError;
            if (!verifyCopy(file, targetPath, plan.verificationMode, &verifyError)) {
                errors.append(verifyError);
                task.state = BackupTaskState::Failed;
                task.errorMessage = verifyError;
                task.statusText = verifyError;
                if (progressCallback) {
                    progressCallback(task);
                }
                writeBackupLog(plan, task, startedAt, QDateTime::currentDateTime().toString(Qt::ISODate), errors);
                return task;
            }
            task.state = BackupTaskState::Running;
        }

        ++task.copiedFiles;
        const auto elapsedMs = qMax<qint64>(1, speedTimer.elapsed());
        task.bytesPerSecond = (static_cast<double>(task.copiedBytes) * 1000.0) / static_cast<double>(elapsedMs);
        task.statusText = QStringLiteral("已复制 %1 / %2 个文件").arg(task.copiedFiles).arg(task.totalFiles);
        if (progressCallback) {
            progressCallback(task);
        }
    }

    task.state = errors.isEmpty() ? BackupTaskState::Completed : BackupTaskState::Warning;
    task.statusText = errors.isEmpty() ? QStringLiteral("备份完成") : QStringLiteral("备份完成，有警告");
    task.errorMessage = errors.join(QStringLiteral("\n"));
    const auto logPath = writeBackupLog(plan, task, startedAt, QDateTime::currentDateTime().toString(Qt::ISODate), errors);
    if (logPath.isEmpty()) {
        task.state = BackupTaskState::Warning;
        task.errorMessage = task.errorMessage.isEmpty()
            ? QStringLiteral("备份完成，但日志写入失败。")
            : task.errorMessage + QStringLiteral("\n备份完成，但日志写入失败。");
        task.statusText = QStringLiteral("备份完成，日志写入失败");
    }
    if (progressCallback) {
        progressCallback(task);
    }
    return task;
}

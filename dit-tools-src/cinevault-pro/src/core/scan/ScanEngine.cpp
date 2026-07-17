#include "core/scan/ScanEngine.h"

#include "core/jobs/JobEngine.h"
#include "core/media/MediaProbeEngine.h"
#include "core/scan/FileTypeService.h"
#include "core/thumbnail/ThumbnailEngine.h"
#include "infrastructure/db/DatabaseManager.h"
#include "shared/FolderPathMetadata.h"
#include "shared/ScopedBackgroundThreadPriority.h"

#include <QtConcurrent>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QMutexLocker>
#include <QScopeGuard>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace {
QString toIsoString(const std::filesystem::file_time_type &fileTime)
{
    using namespace std::chrono;
    const auto systemNow = system_clock::now();
    const auto fileNow = std::filesystem::file_time_type::clock::now();
    const auto translated = time_point_cast<system_clock::duration>(fileTime - fileNow + systemNow);
    return QDateTime::fromSecsSinceEpoch(duration_cast<seconds>(translated.time_since_epoch()).count()).toString(Qt::ISODate);
}

void bindSourceStats(QSqlQuery &query,
                     const ScanBatch &batch,
                     const QString &status,
                     qint64 videoCount,
                     qint64 audioCount,
                     qint64 imageCount,
                     qint64 otherCount,
                     int scanVersion)
{
    query.addBindValue(status);
    query.addBindValue(batch.totalFiles);
    query.addBindValue(batch.totalFolders);
    query.addBindValue(batch.totalSizeBytes);
    query.addBindValue(videoCount);
    query.addBindValue(audioCount);
    query.addBindValue(imageCount);
    query.addBindValue(otherCount);
    query.addBindValue(batch.warningCount);
    if (scanVersion >= 0) {
        query.addBindValue(scanVersion);
    }
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(batch.sourceRootId);
}

JobProgressContext scanProgressContext(const ScanBatch &batch)
{
    JobProgressContext context;
    context.currentStep = 1;
    context.totalSteps = 1;
    context.stepLabel = QStringLiteral("扫描目录");
    context.currentItem = batch.totalFiles;
    context.unitLabel = QStringLiteral("个文件");
    context.extraLabel = QStringLiteral("%1个文件夹").arg(batch.totalFolders);
    return context;
}

FolderNode makeFolderNode(const SourceRoot &sourceRoot,
                          const QString &rootFolderName,
                          const QString &absolutePath,
                          const QString &relativePath)
{
    FolderNode folder;
    folder.sourceRootId = sourceRoot.id;
    folder.name = FolderPathMetadata::folderName(absolutePath, sourceRoot.name);
    folder.absolutePath = absolutePath;
    folder.pathKey = FolderPathMetadata::normalizedPathKey(absolutePath);
    folder.relativePath = FolderPathMetadata::normalizeRelativePath(relativePath);
    folder.parentRelativePath = FolderPathMetadata::parentRelativePath(folder.relativePath);
    folder.depth = FolderPathMetadata::depth(folder.relativePath);
    const auto date = FolderPathMetadata::inferDate(rootFolderName, folder.relativePath);
    folder.normalizedDate = date.normalizedDate;
    folder.dateAnchor = date.anchorRelativePath;
    return folder;
}
}

ScanEngine::ScanEngine(DatabaseManager *databaseManager, JobEngine *jobEngine, MediaProbeEngine *mediaProbeEngine, ThumbnailEngine *thumbnailEngine, QObject *parent)
    : QObject(parent)
    , m_databaseManager(databaseManager)
    , m_jobEngine(jobEngine)
    , m_mediaProbeEngine(mediaProbeEngine)
    , m_thumbnailEngine(thumbnailEngine)
{
}

void ScanEngine::startScan(const SourceRoot &sourceRoot, qint64 jobId)
{
    const auto projectDatabasePath = m_databaseManager
        ? QFileInfo(m_databaseManager->databaseFilePath()).absoluteFilePath()
        : QString();
    if (projectDatabasePath.isEmpty()) {
        const auto message = QStringLiteral("没有可用于扫描的项目数据库");
        if (m_jobEngine) {
            m_jobEngine->failJob(jobId, message);
        }
        emit scanFailed(sourceRoot.id, message);
        emit scanFailedForProject(projectDatabasePath, sourceRoot.id, message);
        return;
    }

    const auto activeScanKey = QStringLiteral("%1|%2")
                                   .arg(FolderPathMetadata::normalizedPathKey(projectDatabasePath))
                                   .arg(sourceRoot.id);
    {
        QMutexLocker locker(&m_activeScansMutex);
        if (m_activeScans.contains(activeScanKey)) {
            const auto message = QStringLiteral("该素材源已有扫描任务正在运行");
            if (m_jobEngine) {
                m_jobEngine->failJob(jobId, message);
            }
            emit scanFailed(sourceRoot.id, message);
            emit scanFailedForProject(projectDatabasePath, sourceRoot.id, message);
            return;
        }
        m_activeScans.insert(activeScanKey);
    }

    auto future = QtConcurrent::run([this, sourceRoot, jobId, projectDatabasePath, activeScanKey]() {
        runScan(sourceRoot, jobId, projectDatabasePath, activeScanKey);
    });
    m_scanFutures.addFuture(future);
}

void ScanEngine::waitForIdle()
{
    m_scanFutures.waitForFinished();
}

void ScanEngine::setFailureAfterEntriesForTesting(qint64 entryCount)
{
    m_failureAfterEntries.store(entryCount);
}

void ScanEngine::releaseActiveScan(const QString &activeScanKey)
{
    QMutexLocker locker(&m_activeScansMutex);
    m_activeScans.remove(activeScanKey);
}

void ScanEngine::runScan(SourceRoot sourceRoot,
                         qint64 jobId,
                         const QString &projectDatabasePath,
                         const QString &activeScanKey)
{
    const ScopedBackgroundThreadPriority backgroundPriority;
    const auto activeScanGuard = qScopeGuard([this, activeScanKey]() {
        releaseActiveScan(activeScanKey);
    });
    const auto connectionName = QStringLiteral("scan_%1_%2").arg(sourceRoot.id).arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QString errorMessage;
    auto db = m_databaseManager->openThreadConnectionForPath(projectDatabasePath,
                                                              connectionName,
                                                              &errorMessage);
    if (!db.isOpen()) {
        QMetaObject::invokeMethod(this, [this, sourceRoot, projectDatabasePath, errorMessage, jobId]() {
            if (m_databaseManager
                && FolderPathMetadata::normalizedPathKey(m_databaseManager->databaseFilePath())
                    == FolderPathMetadata::normalizedPathKey(projectDatabasePath)) {
                if (m_jobEngine) {
                    m_jobEngine->failJob(jobId, errorMessage);
                }
                emit scanFailed(sourceRoot.id, errorMessage);
            }
            emit scanFailedForProject(projectDatabasePath, sourceRoot.id, errorMessage);
        }, Qt::QueuedConnection);
        db = QSqlDatabase();
        m_databaseManager->closeThreadConnection(connectionName);
        return;
    }

    ScanBatch batch;
    batch.sourceRootId = sourceRoot.id;
    qint64 videoCount = 0;
    qint64 audioCount = 0;
    qint64 imageCount = 0;
    qint64 otherCount = 0;

    auto invokeProgress = [this, projectDatabasePath, jobId](const ScanBatch &progressBatch) {
        QMetaObject::invokeMethod(this, [this, projectDatabasePath, progressBatch, jobId]() {
            if (!m_databaseManager
                || FolderPathMetadata::normalizedPathKey(m_databaseManager->databaseFilePath())
                    != FolderPathMetadata::normalizedPathKey(projectDatabasePath)) {
                return;
            }
            if (m_jobEngine) {
                m_jobEngine->updateJob(jobId,
                                       progressBatch.progressPercent,
                                       QStringLiteral("已扫描 %1 个文件夹，%2 个文件")
                                           .arg(progressBatch.totalFolders)
                                           .arg(progressBatch.totalFiles),
                                       scanProgressContext(progressBatch));
            }
            emit scanBatchCommitted(progressBatch);
        }, Qt::QueuedConnection);
    };

    auto setupStageTables = [&]() -> bool {
        const QStringList statements = {
            QStringLiteral("DROP TABLE IF EXISTS temp.scan_asset_stage"),
            QStringLiteral("DROP TABLE IF EXISTS temp.scan_folder_stage"),
            QStringLiteral(
                "CREATE TEMP TABLE scan_asset_stage ("
                "source_root_id INTEGER NOT NULL, path_key TEXT NOT NULL PRIMARY KEY, name TEXT NOT NULL, "
                "extension TEXT, absolute_path TEXT NOT NULL, relative_path TEXT NOT NULL, parent_path TEXT NOT NULL, "
                "asset_type INTEGER NOT NULL, size_bytes INTEGER NOT NULL, modified_at TEXT NOT NULL, "
                "is_readable INTEGER NOT NULL, created_at TEXT NOT NULL)"),
            QStringLiteral(
                "CREATE TEMP TABLE scan_folder_stage ("
                "source_root_id INTEGER NOT NULL, path_key TEXT NOT NULL PRIMARY KEY, name TEXT NOT NULL, "
                "absolute_path TEXT NOT NULL, relative_path TEXT NOT NULL, parent_relative_path TEXT NOT NULL, "
                "depth INTEGER NOT NULL, file_count INTEGER NOT NULL, direct_file_count INTEGER NOT NULL, "
                "recursive_file_count INTEGER NOT NULL, normalized_date TEXT NOT NULL, date_anchor TEXT NOT NULL, "
                "created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"),
            QStringLiteral("CREATE INDEX temp.idx_scan_asset_source ON scan_asset_stage(source_root_id)"),
            QStringLiteral("CREATE INDEX temp.idx_scan_folder_source ON scan_folder_stage(source_root_id)")
        };
        QSqlQuery query(db);
        for (const auto &statement : statements) {
            if (!query.exec(statement)) {
                errorMessage = query.lastError().text();
                return false;
            }
        }
        return true;
    };

    auto stageFiles = [&](const QList<AssetFile> &files, qint64 progressPercent) -> bool {
        if (files.isEmpty()) {
            return true;
        }
        if (!db.transaction()) {
            errorMessage = db.lastError().text();
            return false;
        }
        QSqlQuery assetQuery(db);
        assetQuery.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO temp.scan_asset_stage "
            "(source_root_id, path_key, name, extension, absolute_path, relative_path, parent_path, asset_type, "
            "size_bytes, modified_at, is_readable, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

        for (const auto &file : files) {
            assetQuery.addBindValue(file.sourceRootId);
            assetQuery.addBindValue(FolderPathMetadata::normalizedPathKey(file.absolutePath));
            assetQuery.addBindValue(file.name);
            assetQuery.addBindValue(file.extension);
            assetQuery.addBindValue(file.absolutePath);
            assetQuery.addBindValue(file.relativePath);
            assetQuery.addBindValue(file.parentPath);
            assetQuery.addBindValue(static_cast<int>(file.assetType));
            assetQuery.addBindValue(file.sizeBytes);
            assetQuery.addBindValue(file.modifiedAt);
            assetQuery.addBindValue(file.readable ? 1 : 0);
            assetQuery.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
            if (!assetQuery.exec()) {
                errorMessage = assetQuery.lastError().text();
                db.rollback();
                return false;
            }
            assetQuery.finish();
        }
        if (!db.commit()) {
            errorMessage = db.lastError().text();
            db.rollback();
            return false;
        }
        batch.progressPercent = progressPercent;
        invokeProgress(batch);
        return true;
    };

    auto stageFolders = [&](const QList<FolderNode> &folders) -> bool {
        if (!db.transaction()) {
            errorMessage = db.lastError().text();
            return false;
        }
        QSqlQuery folderQuery(db);
        folderQuery.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO temp.scan_folder_stage "
            "(source_root_id, path_key, name, absolute_path, relative_path, parent_relative_path, depth, file_count, "
            "direct_file_count, recursive_file_count, normalized_date, date_anchor, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        const auto now = QDateTime::currentDateTime().toString(Qt::ISODate);
        for (const auto &folder : folders) {
            folderQuery.addBindValue(folder.sourceRootId);
            folderQuery.addBindValue(folder.pathKey);
            folderQuery.addBindValue(folder.name);
            folderQuery.addBindValue(folder.absolutePath);
            folderQuery.addBindValue(folder.relativePath);
            folderQuery.addBindValue(folder.parentRelativePath);
            folderQuery.addBindValue(folder.depth);
            folderQuery.addBindValue(folder.fileCount);
            folderQuery.addBindValue(folder.directFileCount);
            folderQuery.addBindValue(folder.recursiveFileCount);
            folderQuery.addBindValue(folder.normalizedDate);
            folderQuery.addBindValue(folder.dateAnchor);
            folderQuery.addBindValue(now);
            folderQuery.addBindValue(now);
            if (!folderQuery.exec()) {
                errorMessage = folderQuery.lastError().text();
                db.rollback();
                return false;
            }
            folderQuery.finish();
        }
        if (!db.commit()) {
            errorMessage = db.lastError().text();
            db.rollback();
            return false;
        }
        return true;
    };

    auto executeForSource = [&](const QString &statement) -> bool {
        QSqlQuery query(db);
        query.prepare(statement);
        query.addBindValue(sourceRoot.id);
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
        return true;
    };

    auto reconcileStagedScan = [&]() -> bool {
        if (!db.transaction()) {
            errorMessage = db.lastError().text();
            return false;
        }
        auto rollback = [&]() {
            db.rollback();
            return false;
        };

        const QStringList statements = {
            QStringLiteral(
                "DELETE FROM embedded_metadata WHERE asset_id IN ("
                "SELECT af.id FROM asset_file af JOIN temp.scan_asset_stage s "
                "ON s.source_root_id = af.source_root_id AND s.path_key = af.path_key "
                "WHERE af.source_root_id = ? AND (af.size_bytes <> s.size_bytes OR af.modified_at <> s.modified_at))"),
            QStringLiteral(
                "DELETE FROM media_metadata WHERE asset_id IN ("
                "SELECT af.id FROM asset_file af JOIN temp.scan_asset_stage s "
                "ON s.source_root_id = af.source_root_id AND s.path_key = af.path_key "
                "WHERE af.source_root_id = ? AND (af.size_bytes <> s.size_bytes OR af.modified_at <> s.modified_at))"),
            QStringLiteral(
                "DELETE FROM thumbnail WHERE asset_id IN ("
                "SELECT af.id FROM asset_file af JOIN temp.scan_asset_stage s "
                "ON s.source_root_id = af.source_root_id AND s.path_key = af.path_key "
                "WHERE af.source_root_id = ? AND (af.size_bytes <> s.size_bytes OR af.modified_at <> s.modified_at))"),
            QStringLiteral(
                "UPDATE asset_file SET "
                "name = (SELECT s.name FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "extension = (SELECT s.extension FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "absolute_path = (SELECT s.absolute_path FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "relative_path = (SELECT s.relative_path FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "parent_path = (SELECT s.parent_path FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "asset_type = (SELECT s.asset_type FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "size_bytes = (SELECT s.size_bytes FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "modified_at = (SELECT s.modified_at FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key), "
                "is_readable = (SELECT s.is_readable FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key) "
                "WHERE source_root_id = ? AND EXISTS (SELECT 1 FROM temp.scan_asset_stage s "
                "WHERE s.source_root_id = asset_file.source_root_id AND s.path_key = asset_file.path_key)"),
            QStringLiteral(
                "INSERT INTO asset_file "
                "(source_root_id, name, extension, absolute_path, relative_path, parent_path, path_key, asset_type, "
                "size_bytes, modified_at, is_readable, created_at) "
                "SELECT s.source_root_id, s.name, s.extension, s.absolute_path, s.relative_path, s.parent_path, s.path_key, "
                "s.asset_type, s.size_bytes, s.modified_at, s.is_readable, s.created_at FROM temp.scan_asset_stage s "
                "WHERE s.source_root_id = ? AND NOT EXISTS (SELECT 1 FROM asset_file af "
                "WHERE af.source_root_id = s.source_root_id AND af.path_key = s.path_key)"),
            QStringLiteral(
                "DELETE FROM asset_file WHERE source_root_id = ? AND NOT EXISTS ("
                "SELECT 1 FROM temp.scan_asset_stage s WHERE s.source_root_id = asset_file.source_root_id "
                "AND s.path_key = asset_file.path_key)"),
            QStringLiteral(
                "UPDATE folder_node SET "
                "name = (SELECT s.name FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "absolute_path = (SELECT s.absolute_path FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "relative_path = (SELECT s.relative_path FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "parent_relative_path = (SELECT s.parent_relative_path FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "depth = (SELECT s.depth FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "file_count = (SELECT s.file_count FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "direct_file_count = (SELECT s.direct_file_count FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "recursive_file_count = (SELECT s.recursive_file_count FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "normalized_date = (SELECT s.normalized_date FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "date_anchor = (SELECT s.date_anchor FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key), "
                "updated_at = (SELECT s.updated_at FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key) "
                "WHERE source_root_id = ? AND EXISTS (SELECT 1 FROM temp.scan_folder_stage s "
                "WHERE s.source_root_id = folder_node.source_root_id AND s.path_key = folder_node.path_key)"),
            QStringLiteral(
                "INSERT INTO folder_node "
                "(source_root_id, name, absolute_path, path_key, relative_path, parent_relative_path, depth, file_count, "
                "direct_file_count, recursive_file_count, normalized_date, date_anchor, created_at, updated_at) "
                "SELECT s.source_root_id, s.name, s.absolute_path, s.path_key, s.relative_path, s.parent_relative_path, "
                "s.depth, s.file_count, s.direct_file_count, s.recursive_file_count, s.normalized_date, s.date_anchor, "
                "s.created_at, s.updated_at FROM temp.scan_folder_stage s WHERE s.source_root_id = ? "
                "AND NOT EXISTS (SELECT 1 FROM folder_node fn WHERE fn.source_root_id = s.source_root_id AND fn.path_key = s.path_key)"),
            QStringLiteral(
                "DELETE FROM folder_node WHERE source_root_id = ? AND NOT EXISTS ("
                "SELECT 1 FROM temp.scan_folder_stage s WHERE s.source_root_id = folder_node.source_root_id "
                "AND s.path_key = folder_node.path_key)")
        };
        for (const auto &statement : statements) {
            if (!executeForSource(statement)) {
                return rollback();
            }
        }

        QSqlQuery sourceUpdate(db);
        sourceUpdate.prepare(QStringLiteral(
            "UPDATE source_root SET status = ?, total_files = ?, total_folders = ?, total_size_bytes = ?, "
            "video_count = ?, audio_count = ?, image_count = ?, other_count = ?, warning_count = ?, "
            "scan_version = ?, updated_at = ? WHERE id = ?"));
        bindSourceStats(sourceUpdate,
                        batch,
                        batch.warningCount > 0 ? QStringLiteral("warning") : QStringLiteral("ok"),
                        videoCount,
                        audioCount,
                        imageCount,
                        otherCount,
                        ScanEngine::CurrentScanVersion);
        if (!sourceUpdate.exec()) {
            errorMessage = sourceUpdate.lastError().text();
            return rollback();
        }
        if (!db.commit()) {
            errorMessage = db.lastError().text();
            return rollback();
        }
        return true;
    };

    QList<FolderNode> folders;
    QHash<QString, int> folderIndexes;
    QList<AssetFile> files;
    constexpr qint64 kBatchSize = 500;

    try {
        if (!setupStageTables()) {
            throw std::runtime_error(errorMessage.toStdString());
        }
        const auto rootFolderName = FolderPathMetadata::folderName(sourceRoot.path, sourceRoot.name);
        folders.append(makeFolderNode(sourceRoot, rootFolderName, sourceRoot.path, QStringLiteral("")));
        folderIndexes.insert(QStringLiteral(""), 0);

        const std::filesystem::path rootPath = sourceRoot.path.toStdWString();
        std::error_code iteratorError;
        std::filesystem::recursive_directory_iterator it(
            rootPath,
            std::filesystem::directory_options::skip_permission_denied,
            iteratorError);
        const std::filesystem::recursive_directory_iterator end;
        if (iteratorError) {
            throw std::runtime_error(iteratorError.message());
        }
        while (it != end) {
            const auto path = it->path();
            const auto absolutePath = QString::fromStdWString(path.wstring());
            const auto relativePath = FolderPathMetadata::relativePathFromRoot(sourceRoot.path, absolutePath);
            ++batch.processedEntries;

            std::error_code typeError;
            const auto isDirectory = it->is_directory(typeError);
            if (typeError) {
                ++batch.warningCount;
                typeError.clear();
            } else if (isDirectory) {
                const auto normalizedRelativePath = FolderPathMetadata::normalizeRelativePath(relativePath);
                const auto key = normalizedRelativePath.toCaseFolded();
                if (!folderIndexes.contains(key)) {
                    folderIndexes.insert(key, folders.size());
                    folders.append(makeFolderNode(sourceRoot,
                                                  rootFolderName,
                                                  absolutePath,
                                                  normalizedRelativePath));
                }
                ++batch.totalFolders;
            } else if (it->is_regular_file(typeError) && !typeError) {
                const auto assetType = FileTypeService::classify(QFileInfo(absolutePath).fileName());
                if (assetType != AssetType::Video && assetType != AssetType::Image) {
                    it.increment(iteratorError);
                    if (iteratorError) { ++batch.warningCount; iteratorError.clear(); }
                    continue;
                }
                AssetFile file;
                file.sourceRootId = sourceRoot.id;
                file.name = QFileInfo(absolutePath).fileName();
                file.extension = QFileInfo(absolutePath).suffix().toLower();
                file.absolutePath = absolutePath;
                file.relativePath = FolderPathMetadata::normalizeRelativePath(relativePath);
                file.parentPath = QFileInfo(absolutePath).absolutePath();
                file.assetType = assetType;
                std::error_code metadataError;
                file.sizeBytes = static_cast<qint64>(it->file_size(metadataError));
                if (metadataError) {
                    file.sizeBytes = 0;
                    ++batch.warningCount;
                    metadataError.clear();
                }
                const auto modifiedTime = it->last_write_time(metadataError);
                file.modifiedAt = metadataError ? QString() : toIsoString(modifiedTime);
                if (metadataError) {
                    ++batch.warningCount;
                }
                file.readable = QFileInfo(absolutePath).isReadable();
                files.append(file);

                const auto parentRelativePath = FolderPathMetadata::parentRelativePath(file.relativePath);
                const auto directIndex = folderIndexes.value(parentRelativePath.toCaseFolded(), -1);
                if (directIndex >= 0) {
                    ++folders[directIndex].directFileCount;
                    folders[directIndex].fileCount = folders[directIndex].directFileCount;
                }
                for (const auto &ancestor : FolderPathMetadata::ancestorRelativePaths(parentRelativePath)) {
                    const auto ancestorIndex = folderIndexes.value(ancestor.toCaseFolded(), -1);
                    if (ancestorIndex >= 0) {
                        ++folders[ancestorIndex].recursiveFileCount;
                    }
                }

                ++batch.totalFiles;
                batch.totalSizeBytes += file.sizeBytes;
                if (!file.readable) {
                    ++batch.warningCount;
                }

                switch (file.assetType) {
                case AssetType::Video: ++videoCount; break;
                case AssetType::Audio: ++audioCount; break;
                case AssetType::Image: ++imageCount; break;
                default: ++otherCount; break;
                }
            }

            if (files.size() >= kBatchSize) {
                const auto progress = std::min<qint64>(95, 5 + (batch.totalFiles / 20));
                if (!stageFiles(files, progress)) {
                    throw std::runtime_error(QStringLiteral("扫描暂存写入失败：%1").arg(errorMessage).toStdString());
                }
                files.clear();
            }

            const auto failureAfterEntries = m_failureAfterEntries.load();
            if (failureAfterEntries >= 0 && batch.processedEntries >= failureAfterEntries) {
                throw std::runtime_error("测试注入：扫描在原子切换前失败");
            }

            it.increment(iteratorError);
            if (iteratorError) {
                ++batch.warningCount;
                iteratorError.clear();
            }
        }

        if (!stageFiles(files, 96) || !stageFolders(folders)) {
            throw std::runtime_error(QStringLiteral("扫描暂存收尾失败：%1").arg(errorMessage).toStdString());
        }
        if (!reconcileStagedScan()) {
            throw std::runtime_error(QStringLiteral("扫描结果原子切换失败：%1").arg(errorMessage).toStdString());
        }

        QMetaObject::invokeMethod(this, [this, sourceRoot, projectDatabasePath, batch, jobId]() {
            const auto stillCurrent = m_databaseManager
                && FolderPathMetadata::normalizedPathKey(m_databaseManager->databaseFilePath())
                    == FolderPathMetadata::normalizedPathKey(projectDatabasePath);
            if (stillCurrent) {
                if (m_jobEngine) {
                    m_jobEngine->completeJob(jobId,
                                             QStringLiteral("%1 扫描完成，发现 %2 个文件")
                                                 .arg(sourceRoot.name)
                                                 .arg(batch.totalFiles));
                }
                emit scanFinished(sourceRoot.id);
            }
            emit scanFinishedForProject(projectDatabasePath, sourceRoot.id);
        }, Qt::QueuedConnection);
    } catch (const std::exception &exception) {
        auto message = QString::fromUtf8(exception.what()).trimmed();
        if (message.isEmpty()) {
            message = QStringLiteral("扫描失败");
        }
        QSqlQuery failUpdate(db);
        failUpdate.prepare(QStringLiteral("UPDATE source_root SET status = ?, warning_count = ?, updated_at = ? WHERE id = ?"));
        failUpdate.addBindValue(QStringLiteral("failed"));
        failUpdate.addBindValue(batch.warningCount + 1);
        failUpdate.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        failUpdate.addBindValue(sourceRoot.id);
        failUpdate.exec();

        QMetaObject::invokeMethod(this, [this, sourceRoot, projectDatabasePath, message, jobId]() {
            const auto stillCurrent = m_databaseManager
                && FolderPathMetadata::normalizedPathKey(m_databaseManager->databaseFilePath())
                    == FolderPathMetadata::normalizedPathKey(projectDatabasePath);
            if (stillCurrent) {
                if (m_jobEngine) {
                    m_jobEngine->failJob(jobId, message);
                }
                emit scanFailed(sourceRoot.id, message);
            }
            emit scanFailedForProject(projectDatabasePath, sourceRoot.id, message);
        }, Qt::QueuedConnection);
    }

    db.close();
    db = QSqlDatabase();
    m_databaseManager->closeThreadConnection(connectionName);
}

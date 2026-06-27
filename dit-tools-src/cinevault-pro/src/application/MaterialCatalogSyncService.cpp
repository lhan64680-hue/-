#include "application/MaterialCatalogSyncService.h"

#include "application/ProjectService.h"
#include "core/jobs/JobEngine.h"
#include "infrastructure/db/GlobalDatabaseManager.h"

#include <QtConcurrent>

#include <QDateTime>
#include <QJsonDocument>
#include <QMetaObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

namespace {
struct ExistingVideoState {
    qint64 sizeBytes = 0;
    QString modifiedAt;
    VideoAnalysisStatus analysisStatus = VideoAnalysisStatus::Pending;
    ConfirmationStatus confirmationStatus = ConfirmationStatus::Pending;
    QString errorMessage;
};

bool execQuery(QSqlQuery &query, QString *errorMessage)
{
    if (query.exec()) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

QSqlDatabase openProjectConnection(const QString &databasePath, const QString &connectionName, QString *errorMessage)
{
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(databasePath);
    if (!db.open() && errorMessage) {
        *errorMessage = db.lastError().text();
    }
    return db;
}

void closeProjectConnection(const QString &connectionName)
{
    if (!QSqlDatabase::contains(connectionName)) {
        return;
    }
    {
        auto db = QSqlDatabase::database(connectionName);
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QVector<GlobalVideoAsset> fetchProjectVideos(QSqlDatabase &projectDb, const Project &project, QString *errorMessage)
{
    QVector<GlobalVideoAsset> videos;
    QSqlQuery query(projectDb);
    query.prepare(QStringLiteral(
        "SELECT af.id, af.source_root_id, COALESCE(sr.name, ''), af.name, af.absolute_path, af.relative_path, "
        "af.size_bytes, af.modified_at, COALESCE(mm.duration_ms, 0), COALESCE(th.image_path, '') "
        "FROM asset_file af "
        "LEFT JOIN source_root sr ON sr.id = af.source_root_id "
        "LEFT JOIN media_metadata mm ON mm.asset_id = af.id "
        "LEFT JOIN thumbnail th ON th.asset_id = af.id AND th.status = 1 "
        "WHERE af.asset_type = ? "
        "ORDER BY af.id"));
    query.addBindValue(static_cast<int>(AssetType::Video));
    if (!execQuery(query, errorMessage)) {
        return videos;
    }

    while (query.next()) {
        GlobalVideoAsset asset;
        asset.projectUuid = project.id;
        asset.projectName = project.name;
        asset.projectDatabasePath = project.databasePath;
        asset.assetId = query.value(0).toLongLong();
        asset.sourceRootId = query.value(1).toLongLong();
        asset.sourceRootName = query.value(2).toString();
        asset.fileName = query.value(3).toString();
        asset.absolutePath = query.value(4).toString();
        asset.relativePath = query.value(5).toString();
        asset.sizeBytes = query.value(6).toLongLong();
        asset.modifiedAt = query.value(7).toString();
        asset.durationMs = query.value(8).toLongLong();
        asset.thumbnailPath = query.value(9).toString();
        asset.videoKey = QStringLiteral("%1:%2").arg(project.id).arg(asset.assetId);
        videos.append(asset);
    }
    return videos;
}

QHash<QString, ExistingVideoState> loadExistingStates(QSqlDatabase &globalDb, const QString &projectUuid, QString *errorMessage)
{
    QHash<QString, ExistingVideoState> states;
    QSqlQuery query(globalDb);
    query.prepare(QStringLiteral(
        "SELECT video_key, size_bytes, modified_at, analysis_status, confirmation_status, COALESCE(error_message, '') "
        "FROM global_video_asset WHERE project_uuid = ?"));
    query.addBindValue(projectUuid);
    if (!execQuery(query, errorMessage)) {
        return states;
    }

    while (query.next()) {
        ExistingVideoState state;
        state.sizeBytes = query.value(1).toLongLong();
        state.modifiedAt = query.value(2).toString();
        state.analysisStatus = static_cast<VideoAnalysisStatus>(query.value(3).toInt());
        state.confirmationStatus = static_cast<ConfirmationStatus>(query.value(4).toInt());
        state.errorMessage = query.value(5).toString();
        states.insert(query.value(0).toString(), state);
    }
    return states;
}

bool updateProjectRegistry(QSqlDatabase &globalDb,
                           const Project &project,
                           const QString &status,
                           const QString &errorMessageText,
                           QString *errorMessage)
{
    QSqlQuery query(globalDb);
    query.prepare(QStringLiteral(
        "INSERT INTO project_registry (project_uuid, project_name, project_database_path, last_synced_at, sync_status, error_message) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(project_uuid) DO UPDATE SET "
        "project_name = excluded.project_name, "
        "project_database_path = excluded.project_database_path, "
        "last_synced_at = excluded.last_synced_at, "
        "sync_status = excluded.sync_status, "
        "error_message = excluded.error_message"));
    query.addBindValue(project.id);
    query.addBindValue(project.name);
    query.addBindValue(project.databasePath);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(status);
    query.addBindValue(errorMessageText);
    return execQuery(query, errorMessage);
}

bool deleteFtsRow(QSqlDatabase &globalDb, const QString &videoKey, bool hasFts5, QString *errorMessage)
{
    if (!hasFts5) {
        return true;
    }
    QSqlQuery query(globalDb);
    query.prepare(QStringLiteral("DELETE FROM video_search_fts WHERE video_key = ?"));
    query.addBindValue(videoKey);
    return execQuery(query, errorMessage);
}

bool syncProjectIntoGlobal(QSqlDatabase &globalDb,
                           const Project &project,
                           bool hasFts5,
                           QString *errorMessage)
{
    const auto projectConnectionName = QStringLiteral("sync_project_%1_%2")
        .arg(project.id)
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QString openError;
    auto projectDb = openProjectConnection(project.databasePath, projectConnectionName, &openError);
    if (!projectDb.isOpen()) {
        if (errorMessage) {
            *errorMessage = openError;
        }
        closeProjectConnection(projectConnectionName);
        return false;
    }

    QString fetchError;
    const auto videos = fetchProjectVideos(projectDb, project, &fetchError);
    closeProjectConnection(projectConnectionName);
    if (!fetchError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = fetchError;
        }
        return false;
    }

    const auto now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const auto existingStates = loadExistingStates(globalDb, project.id, errorMessage);
    if (errorMessage && !errorMessage->isEmpty()) {
        return false;
    }

    if (!globalDb.transaction()) {
        if (errorMessage) {
            *errorMessage = globalDb.lastError().text();
        }
        return false;
    }

    if (!updateProjectRegistry(globalDb, project, QStringLiteral("syncing"), QString(), errorMessage)) {
        globalDb.rollback();
        return false;
    }

    QSet<QString> currentKeys;
    QSqlQuery clearFrames(globalDb);
    clearFrames.prepare(QStringLiteral("DELETE FROM video_frame_analysis WHERE video_key = ?"));
    QSqlQuery clearResult(globalDb);
    clearResult.prepare(QStringLiteral("DELETE FROM video_analysis_result WHERE video_key = ?"));
    QSqlQuery upsert(globalDb);
    upsert.prepare(QStringLiteral(
        "INSERT INTO global_video_asset "
        "(video_key, project_uuid, project_name, project_database_path, source_root_id, source_root_name, asset_id, "
        "file_name, absolute_path, relative_path, size_bytes, modified_at, duration_ms, thumbnail_path, "
        "analysis_status, confirmation_status, error_message, last_synced_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(video_key) DO UPDATE SET "
        "project_uuid = excluded.project_uuid, "
        "project_name = excluded.project_name, "
        "project_database_path = excluded.project_database_path, "
        "source_root_id = excluded.source_root_id, "
        "source_root_name = excluded.source_root_name, "
        "asset_id = excluded.asset_id, "
        "file_name = excluded.file_name, "
        "absolute_path = excluded.absolute_path, "
        "relative_path = excluded.relative_path, "
        "size_bytes = excluded.size_bytes, "
        "modified_at = excluded.modified_at, "
        "duration_ms = excluded.duration_ms, "
        "thumbnail_path = excluded.thumbnail_path, "
        "analysis_status = excluded.analysis_status, "
        "confirmation_status = excluded.confirmation_status, "
        "error_message = excluded.error_message, "
        "last_synced_at = excluded.last_synced_at, "
        "updated_at = excluded.updated_at"));

    for (const auto &video : videos) {
        currentKeys.insert(video.videoKey);
        const auto existing = existingStates.value(video.videoKey);
        const bool changed = !existingStates.contains(video.videoKey)
            || existing.sizeBytes != video.sizeBytes
            || existing.modifiedAt != video.modifiedAt;

        if (changed) {
            clearFrames.addBindValue(video.videoKey);
            if (!execQuery(clearFrames, errorMessage)) {
                globalDb.rollback();
                return false;
            }
            clearFrames.finish();

            clearResult.addBindValue(video.videoKey);
            if (!execQuery(clearResult, errorMessage)) {
                globalDb.rollback();
                return false;
            }
            clearResult.finish();

            if (!deleteFtsRow(globalDb, video.videoKey, hasFts5, errorMessage)) {
                globalDb.rollback();
                return false;
            }
        }

        upsert.addBindValue(video.videoKey);
        upsert.addBindValue(project.id);
        upsert.addBindValue(project.name);
        upsert.addBindValue(project.databasePath);
        upsert.addBindValue(video.sourceRootId);
        upsert.addBindValue(video.sourceRootName);
        upsert.addBindValue(video.assetId);
        upsert.addBindValue(video.fileName);
        upsert.addBindValue(video.absolutePath);
        upsert.addBindValue(video.relativePath);
        upsert.addBindValue(video.sizeBytes);
        upsert.addBindValue(video.modifiedAt);
        upsert.addBindValue(video.durationMs);
        upsert.addBindValue(video.thumbnailPath);
        upsert.addBindValue(static_cast<int>(changed ? VideoAnalysisStatus::Pending : existing.analysisStatus));
        upsert.addBindValue(static_cast<int>(changed ? ConfirmationStatus::Pending : existing.confirmationStatus));
        upsert.addBindValue(changed ? QString() : existing.errorMessage);
        upsert.addBindValue(now);
        upsert.addBindValue(now);
        if (!execQuery(upsert, errorMessage)) {
            globalDb.rollback();
            return false;
        }
        upsert.finish();
    }

    for (auto it = existingStates.cbegin(); it != existingStates.cend(); ++it) {
        if (currentKeys.contains(it.key())) {
            continue;
        }

        QSqlQuery deleteAsset(globalDb);
        deleteAsset.prepare(QStringLiteral("DELETE FROM global_video_asset WHERE video_key = ?"));
        deleteAsset.addBindValue(it.key());
        if (!execQuery(deleteAsset, errorMessage)) {
            globalDb.rollback();
            return false;
        }
        if (!deleteFtsRow(globalDb, it.key(), hasFts5, errorMessage)) {
            globalDb.rollback();
            return false;
        }
    }

    if (!updateProjectRegistry(globalDb, project, QStringLiteral("ok"), QString(), errorMessage)) {
        globalDb.rollback();
        return false;
    }

    if (!globalDb.commit()) {
        if (errorMessage) {
            *errorMessage = globalDb.lastError().text();
        }
        globalDb.rollback();
        return false;
    }
    return true;
}

QVector<Project> loadRegisteredProjects(QSqlDatabase &globalDb, QString *errorMessage)
{
    QVector<Project> projects;
    QSqlQuery read(globalDb);
    read.prepare(QStringLiteral(
        "SELECT project_uuid, project_name, project_database_path FROM project_registry ORDER BY project_name COLLATE NOCASE"));
    if (!execQuery(read, errorMessage)) {
        return projects;
    }

    while (read.next()) {
        Project project;
        project.id = read.value(0).toString();
        project.name = read.value(1).toString();
        project.databasePath = read.value(2).toString();
        projects.append(project);
    }
    return projects;
}
}

MaterialCatalogSyncService::MaterialCatalogSyncService(GlobalDatabaseManager *globalDatabaseManager,
                                                       JobEngine *jobEngine,
                                                       ProjectService *projectService,
                                                       QObject *parent)
    : QObject(parent)
    , m_globalDatabaseManager(globalDatabaseManager)
    , m_jobEngine(jobEngine)
    , m_projectService(projectService)
{
}

void MaterialCatalogSyncService::syncCurrentProject()
{
    if (m_syncRunning.exchange(true) || !m_globalDatabaseManager || !m_projectService || !m_projectService->hasOpenProject()) {
        return;
    }

    const auto project = m_projectService->currentProject();
    const auto jobId = m_jobEngine
        ? m_jobEngine->createJob(JobType::GlobalSync,
                                 QStringLiteral("同步全局索引 %1").arg(project.name),
                                 QStringLiteral("准备同步当前项目视频到素材管理中心"))
        : 0;

    auto future = QtConcurrent::run([this, project, jobId]() {
        const auto connectionName = QStringLiteral("global_sync_%1_%2")
            .arg(project.id)
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        QString errorMessage;
        auto db = m_globalDatabaseManager->openThreadConnection(connectionName, &errorMessage);
        if (!db.isOpen()) {
            failJob(jobId, errorMessage);
            m_globalDatabaseManager->closeThreadConnection(connectionName);
            m_syncRunning = false;
            return;
        }

        updateJob(jobId, 25, QStringLiteral("正在读取项目视频索引"));
        if (!syncProjectIntoGlobal(db, project, m_globalDatabaseManager->hasFts5(), &errorMessage)) {
            failJob(jobId, errorMessage);
        } else {
            completeJob(jobId, QStringLiteral("当前项目视频已同步到素材管理中心"));
            notifyCatalogChanged();
        }
        m_globalDatabaseManager->closeThreadConnection(connectionName);
        m_syncRunning = false;
    });
    Q_UNUSED(future);
}

void MaterialCatalogSyncService::rebuildAllProjects()
{
    if (m_syncRunning.exchange(true) || !m_globalDatabaseManager) {
        return;
    }

    const auto jobId = m_jobEngine
        ? m_jobEngine->createJob(JobType::GlobalSync,
                                 QStringLiteral("重建全局素材索引"),
                                 QStringLiteral("准备重建所有已登记项目的视频索引"))
        : 0;

    auto future = QtConcurrent::run([this, jobId]() {
        const auto connectionName = QStringLiteral("global_rebuild_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        QString errorMessage;
        auto db = m_globalDatabaseManager->openThreadConnection(connectionName, &errorMessage);
        if (!db.isOpen()) {
            failJob(jobId, errorMessage);
            m_globalDatabaseManager->closeThreadConnection(connectionName);
            m_syncRunning = false;
            return;
        }

        const auto projects = loadRegisteredProjects(db, &errorMessage);
        if (!errorMessage.isEmpty()) {
            failJob(jobId, errorMessage);
            m_globalDatabaseManager->closeThreadConnection(connectionName);
            m_syncRunning = false;
            return;
        }
        if (projects.isEmpty()) {
            completeJob(jobId, QStringLiteral("没有可重建的已登记项目"));
            m_globalDatabaseManager->closeThreadConnection(connectionName);
            m_syncRunning = false;
            return;
        }

        int successCount = 0;
        int failedCount = 0;
        for (int index = 0; index < projects.size(); ++index) {
            const auto &project = projects.at(index);
            updateJob(jobId,
                      qBound<qint64>(qint64{1},
                                     (static_cast<qint64>(index) * qint64{100}) / static_cast<qint64>(projects.size()),
                                     qint64{99}),
                      QStringLiteral("正在重建：%1").arg(project.name));

            QString syncError;
            if (syncProjectIntoGlobal(db, project, m_globalDatabaseManager->hasFts5(), &syncError)) {
                ++successCount;
            } else {
                ++failedCount;
                updateProjectRegistry(db, project, QStringLiteral("failed"), syncError, nullptr);
            }
        }

        if (failedCount > 0) {
            completeJob(jobId, QStringLiteral("全局索引重建完成，成功 %1 个项目，失败 %2 个项目").arg(successCount).arg(failedCount));
        } else {
            completeJob(jobId, QStringLiteral("全局索引重建完成，共 %1 个项目").arg(successCount));
        }
        notifyCatalogChanged();
        m_globalDatabaseManager->closeThreadConnection(connectionName);
        m_syncRunning = false;
    });
    Q_UNUSED(future);
}

void MaterialCatalogSyncService::updateJob(qint64 jobId, qint64 progress, const QString &detail)
{
    if (!m_jobEngine || jobId <= 0) {
        return;
    }
    QMetaObject::invokeMethod(m_jobEngine, [engine = m_jobEngine, jobId, progress, detail]() {
        engine->updateJob(jobId, progress, detail);
    }, Qt::QueuedConnection);
}

void MaterialCatalogSyncService::completeJob(qint64 jobId, const QString &detail)
{
    if (!m_jobEngine || jobId <= 0) {
        return;
    }
    QMetaObject::invokeMethod(m_jobEngine, [engine = m_jobEngine, jobId, detail]() {
        engine->completeJob(jobId, detail);
    }, Qt::QueuedConnection);
}

void MaterialCatalogSyncService::failJob(qint64 jobId, const QString &errorMessage)
{
    if (!m_jobEngine || jobId <= 0) {
        return;
    }
    QMetaObject::invokeMethod(m_jobEngine, [engine = m_jobEngine, jobId, errorMessage]() {
        engine->failJob(jobId, errorMessage);
    }, Qt::QueuedConnection);
}

void MaterialCatalogSyncService::notifyCatalogChanged()
{
    QMetaObject::invokeMethod(this, [this]() {
        emit catalogChanged();
    }, Qt::QueuedConnection);
}

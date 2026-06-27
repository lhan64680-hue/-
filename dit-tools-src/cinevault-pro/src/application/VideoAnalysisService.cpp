#include "application/VideoAnalysisService.h"

#include "core/thumbnail/ContactSheetBuilder.h"
#include "core/jobs/JobEngine.h"
#include "infrastructure/config/AppSettings.h"
#include "infrastructure/db/GlobalDatabaseManager.h"
#include "infrastructure/ffmpeg/FFmpegAdapter.h"
#include "infrastructure/network/VisionApiClient.h"
#include "shared/Paths.h"

#include <QtConcurrent>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMetaObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

namespace {
struct AnalysisConfig {
    QString baseUrl;
    QString apiKey;
    QString model;
    AnalysisMode mode = AnalysisMode::EveryNFrames;
    int frameInterval = 10;
    int contactSheetFrameCount = 24;
    int timeoutSec = 60;
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

QString toJson(const QStringList &items)
{
    QJsonArray array;
    for (const auto &item : items) {
        array.append(item);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool removeDirectoryContents(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return true;
    }
    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }
    return dir.removeRecursively();
}

QStringList uniqueNormalizedKeys(const QStringList &videoKeys)
{
    QStringList normalizedKeys;
    QSet<QString> seen;
    for (const auto &videoKey : videoKeys) {
        const auto normalizedKey = videoKey.trimmed();
        if (normalizedKey.isEmpty() || seen.contains(normalizedKey)) {
            continue;
        }
        normalizedKeys.append(normalizedKey);
        seen.insert(normalizedKey);
    }
    return normalizedKeys;
}

bool loadVideoAsset(QSqlDatabase &db, const QString &videoKey, GlobalVideoAsset *asset, QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT video_key, project_uuid, project_name, project_database_path, source_root_id, source_root_name, asset_id, "
        "file_name, absolute_path, relative_path, size_bytes, modified_at, duration_ms, COALESCE(thumbnail_path, ''), "
        "analysis_status, confirmation_status, COALESCE(error_message, ''), updated_at "
        "FROM global_video_asset WHERE video_key = ?"));
    query.addBindValue(videoKey);
    if (!execQuery(query, errorMessage) || !query.next()) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("素材管理中心找不到该视频");
        }
        return false;
    }

    asset->videoKey = query.value(0).toString();
    asset->projectUuid = query.value(1).toString();
    asset->projectName = query.value(2).toString();
    asset->projectDatabasePath = query.value(3).toString();
    asset->sourceRootId = query.value(4).toLongLong();
    asset->sourceRootName = query.value(5).toString();
    asset->assetId = query.value(6).toLongLong();
    asset->fileName = query.value(7).toString();
    asset->absolutePath = query.value(8).toString();
    asset->relativePath = query.value(9).toString();
    asset->sizeBytes = query.value(10).toLongLong();
    asset->modifiedAt = query.value(11).toString();
    asset->durationMs = query.value(12).toLongLong();
    asset->thumbnailPath = query.value(13).toString();
    asset->analysisStatus = static_cast<VideoAnalysisStatus>(query.value(14).toInt());
    asset->confirmationStatus = static_cast<ConfirmationStatus>(query.value(15).toInt());
    asset->errorMessage = query.value(16).toString();
    asset->updatedAt = query.value(17).toString();
    return true;
}

bool updateAssetState(QSqlDatabase &db,
                      const QString &videoKey,
                      VideoAnalysisStatus analysisStatus,
                      ConfirmationStatus confirmationStatus,
                      const QString &errorMessageText,
                      QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE global_video_asset SET analysis_status = ?, confirmation_status = ?, error_message = ?, updated_at = ? "
        "WHERE video_key = ?"));
    query.addBindValue(static_cast<int>(analysisStatus));
    query.addBindValue(static_cast<int>(confirmationStatus));
    query.addBindValue(errorMessageText);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(videoKey);
    return execQuery(query, errorMessage);
}

bool confirmVideoRecords(QSqlDatabase &db,
                         const QString &videoKey,
                         const QString &confirmedAt,
                         QString *errorMessage)
{
    QSqlQuery result(db);
    result.prepare(QStringLiteral("UPDATE video_analysis_result SET confirmed_at = ? WHERE video_key = ?"));
    result.addBindValue(confirmedAt);
    result.addBindValue(videoKey);
    if (!execQuery(result, errorMessage)) {
        return false;
    }

    QSqlQuery asset(db);
    asset.prepare(QStringLiteral(
        "UPDATE global_video_asset SET confirmation_status = ?, error_message = '', updated_at = ? "
        "WHERE video_key = ?"));
    asset.addBindValue(static_cast<int>(ConfirmationStatus::Confirmed));
    asset.addBindValue(confirmedAt);
    asset.addBindValue(videoKey);
    return execQuery(asset, errorMessage);
}

bool deleteAnalysisArtifacts(QSqlDatabase &db, const QString &videoKey, bool hasFts5, QString *errorMessage)
{
    QSqlQuery frames(db);
    frames.prepare(QStringLiteral("DELETE FROM video_frame_analysis WHERE video_key = ?"));
    frames.addBindValue(videoKey);
    if (!execQuery(frames, errorMessage)) {
        return false;
    }

    QSqlQuery result(db);
    result.prepare(QStringLiteral("DELETE FROM video_analysis_result WHERE video_key = ?"));
    result.addBindValue(videoKey);
    if (!execQuery(result, errorMessage)) {
        return false;
    }

    if (hasFts5) {
        QSqlQuery fts(db);
        fts.prepare(QStringLiteral("DELETE FROM video_search_fts WHERE video_key = ?"));
        fts.addBindValue(videoKey);
        if (!execQuery(fts, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool insertFrameRow(QSqlDatabase &db, const QString &videoKey, const ExtractedFrame &frame, QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO video_frame_analysis "
        "(video_key, frame_number, timestamp_ms, image_path, caption, tags_json, objects_json, actions, setting_text, error_message) "
        "VALUES (?, ?, ?, ?, '', '[]', '[]', '', '', '')"));
    query.addBindValue(videoKey);
    query.addBindValue(frame.frameNumber);
    query.addBindValue(frame.timestampMs);
    query.addBindValue(frame.imagePath);
    return execQuery(query, errorMessage);
}

bool updateFrameAnalysis(QSqlDatabase &db,
                         const QString &videoKey,
                         const FrameAnalysisRecord &frame,
                         QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE video_frame_analysis SET image_path = ?, caption = ?, tags_json = ?, objects_json = ?, "
        "actions = ?, setting_text = ?, error_message = ? WHERE video_key = ? AND frame_number = ?"));
    query.addBindValue(frame.imagePath);
    query.addBindValue(frame.caption);
    query.addBindValue(toJson(frame.tags));
    query.addBindValue(toJson(frame.objects));
    query.addBindValue(frame.actions);
    query.addBindValue(frame.setting);
    query.addBindValue(frame.errorMessage);
    query.addBindValue(videoKey);
    query.addBindValue(frame.frameNumber);
    return execQuery(query, errorMessage);
}

QString buildSearchText(const VisionVideoSummary &summary, const QVector<FrameAnalysisRecord> &frames)
{
    QStringList parts;
    if (!summary.summary.trimmed().isEmpty()) {
        parts.append(summary.summary.trimmed());
    }
    if (!summary.keywords.isEmpty()) {
        parts.append(summary.keywords.join(QStringLiteral(" ")));
    }
    if (!summary.scenes.isEmpty()) {
        parts.append(summary.scenes.join(QStringLiteral(" ")));
    }
    for (const auto &frame : frames) {
        if (!frame.caption.trimmed().isEmpty()) {
            parts.append(frame.caption.trimmed());
        }
        if (!frame.tags.isEmpty()) {
            parts.append(frame.tags.join(QStringLiteral(" ")));
        }
        if (!frame.objects.isEmpty()) {
            parts.append(frame.objects.join(QStringLiteral(" ")));
        }
        if (!frame.actions.trimmed().isEmpty()) {
            parts.append(frame.actions.trimmed());
        }
        if (!frame.setting.trimmed().isEmpty()) {
            parts.append(frame.setting.trimmed());
        }
    }
    return parts.join(QStringLiteral(" "));
}

bool persistSummary(QSqlDatabase &db,
                    const GlobalVideoAsset &asset,
                    const VisionVideoSummary &summary,
                    const QVector<FrameAnalysisRecord> &frames,
                    bool hasFts5,
                    QString *errorMessage)
{
    const auto analyzedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    const auto searchText = buildSearchText(summary, frames);

    QSqlQuery result(db);
    result.prepare(QStringLiteral(
        "INSERT INTO video_analysis_result "
        "(video_key, summary, keywords_json, scenes_json, search_text, model_name, prompt_version, analyzed_at, confirmed_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(video_key) DO UPDATE SET "
        "summary = excluded.summary, "
        "keywords_json = excluded.keywords_json, "
        "scenes_json = excluded.scenes_json, "
        "search_text = excluded.search_text, "
        "model_name = excluded.model_name, "
        "prompt_version = excluded.prompt_version, "
        "analyzed_at = excluded.analyzed_at"));
    result.addBindValue(asset.videoKey);
    result.addBindValue(summary.summary);
    result.addBindValue(toJson(summary.keywords));
    result.addBindValue(toJson(summary.scenes));
    result.addBindValue(searchText);
    result.addBindValue(QStringLiteral("openai-compatible"));
    result.addBindValue(QStringLiteral("v2-detailed-frame-search"));
    result.addBindValue(analyzedAt);
    result.addBindValue(QString());
    if (!execQuery(result, errorMessage)) {
        return false;
    }

    QSqlQuery updateAsset(db);
    updateAsset.prepare(QStringLiteral(
        "UPDATE global_video_asset SET analysis_status = ?, confirmation_status = ?, error_message = '', updated_at = ? "
        "WHERE video_key = ?"));
    updateAsset.addBindValue(static_cast<int>(VideoAnalysisStatus::Ready));
    updateAsset.addBindValue(static_cast<int>(ConfirmationStatus::Pending));
    updateAsset.addBindValue(analyzedAt);
    updateAsset.addBindValue(asset.videoKey);
    if (!execQuery(updateAsset, errorMessage)) {
        return false;
    }

    if (hasFts5) {
        QSqlQuery deleteFts(db);
        deleteFts.prepare(QStringLiteral("DELETE FROM video_search_fts WHERE video_key = ?"));
        deleteFts.addBindValue(asset.videoKey);
        if (!execQuery(deleteFts, errorMessage)) {
            return false;
        }

        QStringList frameTexts;
        for (const auto &frame : frames) {
            QStringList frameParts;
            if (!frame.caption.trimmed().isEmpty()) {
                frameParts.append(frame.caption.trimmed());
            }
            if (!frame.tags.isEmpty()) {
                frameParts.append(frame.tags.join(QStringLiteral(" ")));
            }
            if (!frame.objects.isEmpty()) {
                frameParts.append(frame.objects.join(QStringLiteral(" ")));
            }
            if (!frame.actions.trimmed().isEmpty()) {
                frameParts.append(frame.actions.trimmed());
            }
            if (!frame.setting.trimmed().isEmpty()) {
                frameParts.append(frame.setting.trimmed());
            }
            if (!frameParts.isEmpty()) {
                frameTexts.append(frameParts.join(QStringLiteral(" ")));
            }
        }

        QSqlQuery insertFts(db);
        insertFts.prepare(QStringLiteral(
            "INSERT INTO video_search_fts "
            "(video_key, project_name, source_root_name, file_name, relative_path, summary, keywords, captions) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        insertFts.addBindValue(asset.videoKey);
        insertFts.addBindValue(asset.projectName);
        insertFts.addBindValue(asset.sourceRootName);
        insertFts.addBindValue(asset.fileName);
        insertFts.addBindValue(asset.relativePath);
        insertFts.addBindValue(summary.summary);
        insertFts.addBindValue(summary.keywords.join(QStringLiteral(" ")));
        insertFts.addBindValue(frameTexts.join(QStringLiteral(" ")));
        if (!execQuery(insertFts, errorMessage)) {
            return false;
        }
    }
    return true;
}

AnalysisConfig loadConfig(const AppSettings *settings)
{
    AnalysisConfig config;
    if (!settings) {
        return config;
    }
    config.baseUrl = settings->visionBaseUrl();
    config.apiKey = settings->visionApiKey();
    config.model = settings->visionModel();
    config.mode = settings->analysisMode();
    config.frameInterval = settings->frameInterval();
    config.contactSheetFrameCount = settings->contactSheetFrameCount();
    config.timeoutSec = settings->analysisTimeoutSec();
    return config;
}
}

VideoAnalysisService::VideoAnalysisService(GlobalDatabaseManager *globalDatabaseManager,
                                           JobEngine *jobEngine,
                                           AppSettings *settings,
                                           FFmpegAdapter *ffmpegAdapter,
                                           VisionApiClient *visionApiClient,
                                           QObject *parent)
    : QObject(parent)
    , m_globalDatabaseManager(globalDatabaseManager)
    , m_jobEngine(jobEngine)
    , m_settings(settings)
    , m_ffmpegAdapter(ffmpegAdapter)
    , m_visionApiClient(visionApiClient)
{
}

bool VideoAnalysisService::validateReadyForEnqueue(const QString &videoKey, QString *errorMessage) const
{
    const auto normalizedKey = videoKey.trimmed();
    if (normalizedKey.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先选择一个视频素材。");
        }
        return false;
    }
    if (!m_globalDatabaseManager || !m_globalDatabaseManager->isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("素材管理中心数据库未打开，请先同步当前项目。");
        }
        return false;
    }
    if (!m_ffmpegAdapter || !m_ffmpegAdapter->isAvailable()) {
        if (errorMessage) {
            *errorMessage = m_ffmpegAdapter
                ? m_ffmpegAdapter->unavailableReason()
                : QStringLiteral("FFmpeg 服务不可用。");
        }
        return false;
    }
    if (!m_visionApiClient) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉解析客户端不可用。");
        }
        return false;
    }

    const auto config = loadConfig(m_settings);
    if (config.baseUrl.isEmpty() || config.apiKey.isEmpty() || config.model.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先在设置中完整填写 Base URL、API Key 和模型名，并点击“保存并应用”。");
        }
        return false;
    }
    if (m_currentVideoKey == normalizedKey || m_queuedVideoKeys.contains(normalizedKey)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("该视频已在解析队列中。");
        }
        return false;
    }
    return true;
}

bool VideoAnalysisService::enqueueVideo(const QString &videoKey, QString *errorMessage)
{
    const auto normalizedKey = videoKey.trimmed();
    if (!validateReadyForEnqueue(normalizedKey, errorMessage)) {
        return false;
    }

    m_analysisQueue.enqueue(normalizedKey);
    m_queuedVideoKeys.insert(normalizedKey);
    reportAnalysisProgress(normalizedKey, 0, QStringLiteral("等待解析队列"), JobState::Pending);
    emit analysisQueueChanged(m_currentVideoKey, m_analysisQueue.size());
    startNextAnalysis();
    return true;
}

int VideoAnalysisService::enqueueVideos(const QStringList &videoKeys, QString *errorMessage)
{
    int accepted = 0;
    QSet<QString> seen;
    QStringList rejectedMessages;
    for (const auto &videoKey : videoKeys) {
        const auto normalizedKey = videoKey.trimmed();
        if (normalizedKey.isEmpty() || seen.contains(normalizedKey)) {
            continue;
        }
        seen.insert(normalizedKey);

        QString rejection;
        if (enqueueVideo(normalizedKey, &rejection)) {
            ++accepted;
        } else if (!rejection.trimmed().isEmpty()) {
            rejectedMessages.append(rejection);
        }
    }

    if (errorMessage) {
        if (accepted > 0) {
            *errorMessage = QStringLiteral("已加入 %1 条视频到解析队列。").arg(accepted);
        } else if (!rejectedMessages.isEmpty()) {
            *errorMessage = rejectedMessages.first();
        } else {
            *errorMessage = QStringLiteral("当前结果中没有可解析的视频。");
        }
    }
    return accepted;
}

void VideoAnalysisService::analyzeVideo(const QString &videoKey)
{
    QString errorMessage;
    enqueueVideo(videoKey, &errorMessage);
}

void VideoAnalysisService::startNextAnalysis()
{
    if (m_analysisRunning) {
        return;
    }
    if (m_analysisQueue.isEmpty()) {
        emit analysisQueueChanged(QString(), 0);
        return;
    }

    const auto normalizedKey = m_analysisQueue.dequeue();
    m_queuedVideoKeys.remove(normalizedKey);
    m_currentVideoKey = normalizedKey;
    m_analysisRunning = true;
    emit analysisQueueChanged(m_currentVideoKey, m_analysisQueue.size());

    const auto config = loadConfig(m_settings);
    if (config.baseUrl.isEmpty() || config.apiKey.isEmpty() || config.model.isEmpty()) {
        const auto errorMessage = QStringLiteral("视觉接口参数不完整，请在设置中保存并应用后重试。");
        reportAnalysisProgress(normalizedKey, 0, errorMessage, JobState::Failed, errorMessage);
        finishCurrentAnalysis(normalizedKey);
        return;
    }

    const auto jobId = m_jobEngine
        ? m_jobEngine->createJob(JobType::ContentAnalysis,
                                 QStringLiteral("视频内容解析"),
                                 QStringLiteral("准备解析视频素材内容"))
        : 0;
    reportAnalysisProgress(normalizedKey, 0, QStringLiteral("准备解析视频素材内容"), JobState::Running);

    auto future = QtConcurrent::run([this, normalizedKey, config, jobId]() {
        const auto connectionName = QStringLiteral("video_analysis_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        QString errorMessage;
        qint64 lastProgress = 0;

        auto finishFailure = [&](const QString &message, QSqlDatabase *db = nullptr, bool updateAsset = true) {
            const auto normalizedMessage = message.trimmed().isEmpty()
                ? QStringLiteral("解析失败。")
                : message.trimmed();
            if (updateAsset && db && db->isOpen()) {
                updateAssetState(*db,
                                 normalizedKey,
                                 VideoAnalysisStatus::Failed,
                                 ConfirmationStatus::Pending,
                                 normalizedMessage,
                                 nullptr);
            }
            failJob(jobId, normalizedMessage);
            reportAnalysisProgress(normalizedKey, lastProgress, normalizedMessage, JobState::Failed, normalizedMessage);
            notifyCatalogChanged();
            m_globalDatabaseManager->closeThreadConnection(connectionName);
            QMetaObject::invokeMethod(this, [this, normalizedKey]() {
                finishCurrentAnalysis(normalizedKey);
            }, Qt::QueuedConnection);
        };

        auto updateRunning = [&](qint64 progress, const QString &detail) {
            lastProgress = progress;
            updateJob(jobId, progress, detail);
            reportAnalysisProgress(normalizedKey, progress, detail, JobState::Running);
        };

        auto db = m_globalDatabaseManager->openThreadConnection(connectionName, &errorMessage);
        if (!db.isOpen()) {
            finishFailure(errorMessage, nullptr, false);
            return;
        }

        GlobalVideoAsset asset;
        if (!loadVideoAsset(db, normalizedKey, &asset, &errorMessage)) {
            finishFailure(errorMessage, &db, false);
            return;
        }

        updateRunning(5, QStringLiteral("准备抽取视频帧：%1").arg(asset.fileName));
        if (!db.transaction()) {
            finishFailure(db.lastError().text(), &db);
            return;
        }
        if (!deleteAnalysisArtifacts(db, normalizedKey, m_globalDatabaseManager->hasFts5(), &errorMessage)
            || !updateAssetState(db, normalizedKey, VideoAnalysisStatus::Running, ConfirmationStatus::Pending, QString(), &errorMessage)) {
            db.rollback();
            finishFailure(errorMessage, &db);
            return;
        }
        if (!db.commit()) {
            finishFailure(db.lastError().text(), &db);
            return;
        }
        notifyCatalogChanged();

        const auto cacheDirectory = Paths::projectFrameCacheDirectory(asset.projectDatabasePath, normalizedKey);
        removeDirectoryContents(cacheDirectory);

        FrameExtractionRequest request;
        request.assetId = asset.assetId;
        request.sourcePath = asset.absolutePath;
        request.outputDirectory = cacheDirectory;
        request.mode = config.mode;
        request.frameInterval = config.frameInterval;

        updateRunning(8, QStringLiteral("正在抽取视频帧：%1").arg(asset.fileName));
        const auto extraction = m_ffmpegAdapter->extractFrames(request);
        if (!extraction.success || extraction.frames.isEmpty()) {
            updateAssetState(db, normalizedKey, VideoAnalysisStatus::Failed, ConfirmationStatus::Pending, extraction.errorMessage, nullptr);
            finishFailure(extraction.errorMessage, &db, false);
            return;
        }

        if (!db.transaction()) {
            finishFailure(db.lastError().text(), &db);
            return;
        }
        for (const auto &frame : extraction.frames) {
            if (!insertFrameRow(db, normalizedKey, frame, &errorMessage)) {
                db.rollback();
                updateAssetState(db, normalizedKey, VideoAnalysisStatus::Failed, ConfirmationStatus::Pending, errorMessage, nullptr);
                finishFailure(errorMessage, &db, false);
                return;
            }
        }
        if (!db.commit()) {
            finishFailure(db.lastError().text(), &db);
            return;
        }

        QStringList contactSheetFrames;
        contactSheetFrames.reserve(extraction.frames.size());
        for (const auto &frame : extraction.frames) {
            contactSheetFrames.append(frame.imagePath);
        }
        ContactSheetBuilder::build(contactSheetFrames,
                                   config.contactSheetFrameCount,
                                   Paths::projectContactSheetPath(asset.projectDatabasePath, normalizedKey, config.contactSheetFrameCount));
        updateRunning(10, QStringLiteral("已抽取 %1 帧，开始视觉解析").arg(extraction.frames.size()));

        QVector<FrameAnalysisRecord> analyzedFrames;
        analyzedFrames.reserve(extraction.frames.size());
        int failedFrames = 0;
        for (int index = 0; index < extraction.frames.size(); ++index) {
            const auto &frame = extraction.frames.at(index);
            FrameAnalysisRecord record;
            record.videoKey = normalizedKey;
            record.frameNumber = frame.frameNumber;
            record.timestampMs = frame.timestampMs;
            record.imagePath = frame.imagePath;

            QString frameError;
            const auto analysis = m_visionApiClient->analyzeFrame(frame.imagePath,
                                                                  config.baseUrl,
                                                                  config.apiKey,
                                                                  config.model,
                                                                  config.timeoutSec,
                                                                  &frameError);
            if (!analysis.has_value()) {
                ++failedFrames;
                record.errorMessage = frameError;
            } else {
                record.caption = analysis->caption;
                record.tags = analysis->tags;
                record.objects = analysis->objects;
                record.actions = analysis->actions;
                record.setting = analysis->setting;
            }
            analyzedFrames.append(record);

            if (!updateFrameAnalysis(db, normalizedKey, record, &errorMessage)) {
                updateAssetState(db, normalizedKey, VideoAnalysisStatus::Failed, ConfirmationStatus::Pending, errorMessage, nullptr);
                finishFailure(errorMessage, &db, false);
                return;
            }

            const auto progress = 10 + ((static_cast<qint64>(index + 1) * 75) / extraction.frames.size());
            updateRunning(progress,
                          QStringLiteral("正在解析视频帧 %1/%2，失败 %3 帧")
                              .arg(index + 1)
                              .arg(extraction.frames.size())
                              .arg(failedFrames));
        }

        QVector<FrameAnalysisRecord> successfulFrames;
        for (const auto &frame : analyzedFrames) {
            if (frame.errorMessage.trimmed().isEmpty()) {
                successfulFrames.append(frame);
            }
        }
        if (successfulFrames.isEmpty()) {
            const auto failMessage = QStringLiteral("所有视频帧都解析失败");
            updateAssetState(db, normalizedKey, VideoAnalysisStatus::Failed, ConfirmationStatus::Pending, failMessage, nullptr);
            finishFailure(failMessage, &db, false);
            return;
        }

        updateRunning(90, QStringLiteral("正在汇总视频内容"));
        QString summaryError;
        const auto summary = m_visionApiClient->summarizeVideo(asset.fileName,
                                                               successfulFrames,
                                                               config.baseUrl,
                                                               config.apiKey,
                                                               config.model,
                                                               config.timeoutSec,
                                                               &summaryError);
        if (!summary.has_value()) {
            updateAssetState(db, normalizedKey, VideoAnalysisStatus::Failed, ConfirmationStatus::Pending, summaryError, nullptr);
            finishFailure(summaryError, &db, false);
            return;
        }

        if (!db.transaction()) {
            finishFailure(db.lastError().text(), &db);
            return;
        }
        if (!persistSummary(db, asset, *summary, analyzedFrames, m_globalDatabaseManager->hasFts5(), &errorMessage)) {
            db.rollback();
            updateAssetState(db, normalizedKey, VideoAnalysisStatus::Failed, ConfirmationStatus::Pending, errorMessage, nullptr);
            finishFailure(errorMessage, &db, false);
            return;
        }
        if (!db.commit()) {
            finishFailure(db.lastError().text(), &db);
            return;
        }

        const auto successMessage = failedFrames > 0
            ? QStringLiteral("视频解析完成，成功 %1 帧，失败 %2 帧，等待确认").arg(successfulFrames.size()).arg(failedFrames)
            : QStringLiteral("视频解析完成，等待确认");
        completeJob(jobId, successMessage);
        reportAnalysisProgress(normalizedKey, 100, successMessage, JobState::Completed);
        notifyCatalogChanged();
        m_globalDatabaseManager->closeThreadConnection(connectionName);
        QMetaObject::invokeMethod(this, [this, normalizedKey]() {
            finishCurrentAnalysis(normalizedKey);
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

void VideoAnalysisService::finishCurrentAnalysis(const QString &videoKey)
{
    if (m_currentVideoKey != videoKey) {
        return;
    }
    m_currentVideoKey.clear();
    m_analysisRunning = false;
    emit analysisQueueChanged(m_currentVideoKey, m_analysisQueue.size());
    startNextAnalysis();
}

bool VideoAnalysisService::confirmVideo(const QString &videoKey)
{
    return confirmVideos(QStringList{videoKey}) > 0;
}

int VideoAnalysisService::confirmVideos(const QStringList &videoKeys)
{
    if (!m_globalDatabaseManager || !m_globalDatabaseManager->isOpen()) {
        return 0;
    }

    const auto normalizedKeys = uniqueNormalizedKeys(videoKeys);
    if (normalizedKeys.isEmpty()) {
        return 0;
    }

    QString errorMessage;
    const auto confirmedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    auto db = m_globalDatabaseManager->database();
    if (!db.transaction()) {
        return 0;
    }

    for (const auto &normalizedKey : normalizedKeys) {
        if (!confirmVideoRecords(db, normalizedKey, confirmedAt, &errorMessage)) {
            db.rollback();
            return 0;
        }
    }

    if (!db.commit()) {
        db.rollback();
        return 0;
    }

    emit catalogChanged();
    return normalizedKeys.size();
}

void VideoAnalysisService::reportAnalysisProgress(const QString &videoKey,
                                                  qint64 progress,
                                                  const QString &detail,
                                                  JobState state,
                                                  const QString &errorMessage)
{
    const auto normalizedProgress = qBound<qint64>(0LL, progress, 100LL);
    if (QThread::currentThread() == thread()) {
        emit analysisProgressChanged(videoKey, normalizedProgress, detail, static_cast<int>(state), errorMessage);
        return;
    }

    QMetaObject::invokeMethod(this, [this, videoKey, normalizedProgress, detail, state, errorMessage]() {
        emit analysisProgressChanged(videoKey, normalizedProgress, detail, static_cast<int>(state), errorMessage);
    }, Qt::QueuedConnection);
}

void VideoAnalysisService::updateJob(qint64 jobId, qint64 progress, const QString &detail)
{
    if (!m_jobEngine || jobId <= 0) {
        return;
    }
    QMetaObject::invokeMethod(m_jobEngine, [engine = m_jobEngine, jobId, progress, detail]() {
        engine->updateJob(jobId, progress, detail);
    }, Qt::QueuedConnection);
}

void VideoAnalysisService::completeJob(qint64 jobId, const QString &detail)
{
    if (!m_jobEngine || jobId <= 0) {
        return;
    }
    QMetaObject::invokeMethod(m_jobEngine, [engine = m_jobEngine, jobId, detail]() {
        engine->completeJob(jobId, detail);
    }, Qt::QueuedConnection);
}

void VideoAnalysisService::failJob(qint64 jobId, const QString &errorMessage)
{
    if (!m_jobEngine || jobId <= 0) {
        return;
    }
    QMetaObject::invokeMethod(m_jobEngine, [engine = m_jobEngine, jobId, errorMessage]() {
        engine->failJob(jobId, errorMessage);
    }, Qt::QueuedConnection);
}

void VideoAnalysisService::notifyCatalogChanged()
{
    QMetaObject::invokeMethod(this, [this]() {
        emit catalogChanged();
    }, Qt::QueuedConnection);
}

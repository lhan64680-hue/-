#include "ui/viewmodels/MaterialCenterViewModel.h"

#include "application/MaterialCatalogSyncService.h"
#include "application/MaterialCenterQueryService.h"
#include "application/ProjectService.h"
#include "application/SearchDocumentSyncService.h"
#include "application/VideoAnalysisService.h"
#include "core/search/SearchQueryUnderstanding.h"
#include "core/thumbnail/ContactSheetBuilder.h"
#include "infrastructure/config/AppSettings.h"
#include "infrastructure/network/VisionApiClient.h"
#include "shared/Formatters.h"
#include "shared/Paths.h"
#include "shared/VisualAnalysisMetadata.h"
#include "ui/models/MaterialCenterFolderListModel.h"
#include "ui/models/MaterialCenterFrameListModel.h"
#include "ui/models/MaterialCenterListModel.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QFile>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <optional>

namespace {
constexpr int kInitialVisibleFrameCount = 24;
constexpr int kVisibleFrameBatchSize = 24;
constexpr int kMaxFrameRetryCount = 3;

QVariantList prependAllOption(const QVariantList &items, const QString &label)
{
    QVariantList options;
    options.append(QVariantMap{{QStringLiteral("value"), QString()}, {QStringLiteral("label"), label}});
    for (const auto &item : items) {
        options.append(item);
    }
    return options;
}

QVariantList stringListToVariants(const QStringList &items)
{
    QVariantList values;
    for (const auto &item : items) {
        values.append(item);
    }
    return values;
}

QVariantList dimensionAnalysesToVariants(const QVector<MaterialDimensionAnalysis> &items)
{
    QVariantList values;
    for (const auto &item : items) {
        values.append(QVariantMap{
            {QStringLiteral("name"), item.name},
            {QStringLiteral("detail"), item.detail},
            {QStringLiteral("analyzedAt"), item.analyzedAt}
        });
    }
    return values;
}

QStringList variantListToStringList(const QVariantList &items)
{
    QStringList values;
    for (const auto &item : items) {
        const auto text = item.toString().simplified();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    values.removeDuplicates();
    return values;
}

QStringList searchTerms(const QString &text)
{
    return text.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

QVariantMap assetTypeOption(int value, const QString &label)
{
    return QVariantMap{{QStringLiteral("value"), value}, {QStringLiteral("label"), label}};
}

QVariantList fileTypeOptions()
{
    return {
        assetTypeOption(-1, QStringLiteral("全部文件")),
        assetTypeOption(static_cast<int>(AssetType::Video), QStringLiteral("视频文件")),
        assetTypeOption(static_cast<int>(AssetType::Audio), QStringLiteral("音频文件")),
        assetTypeOption(static_cast<int>(AssetType::Image), QStringLiteral("图片文件")),
        assetTypeOption(static_cast<int>(AssetType::Subtitle), QStringLiteral("字幕文件")),
        assetTypeOption(static_cast<int>(AssetType::ProjectFile), QStringLiteral("工程文件")),
        assetTypeOption(static_cast<int>(AssetType::Document), QStringLiteral("文档文件")),
        assetTypeOption(static_cast<int>(AssetType::Archive), QStringLiteral("压缩文件")),
        assetTypeOption(static_cast<int>(AssetType::Other), QStringLiteral("其他文件")),
        assetTypeOption(static_cast<int>(AssetType::Unknown), QStringLiteral("未识别文件"))
    };
}

QString frameSearchText(const FrameAnalysisRecord &frame)
{
    const auto entityTerms = VisualAnalysisMetadata::entityFactSearchTerms(frame.entities);
    return QStringList{
        frame.caption,
        frame.tags.join(QStringLiteral(" ")),
        frame.objects.join(QStringLiteral(" ")),
        frame.actions,
        frame.setting,
        entityTerms.join(QStringLiteral(" ")),
        frame.ocrText,
        frame.errorMessage
    }.join(QStringLiteral(" "));
}

QStringList matchedFrameTerms(const FrameAnalysisRecord &frame, const QStringList &terms)
{
    QStringList matches;
    const auto haystack = frameSearchText(frame);
    for (const auto &term : terms) {
        if (haystack.contains(term, Qt::CaseInsensitive)) {
            matches.append(term);
        }
    }
    matches.removeDuplicates();
    return matches;
}

bool frameMatches(const FrameAnalysisRecord &frame, const QStringList &terms)
{
    return terms.isEmpty() || !matchedFrameTerms(frame, terms).isEmpty();
}

int persistedAnalysisProgress(const GlobalVideoAsset &asset)
{
    if (asset.analysisStatus == VideoAnalysisStatus::Ready) {
        return 100;
    }

    const auto &task = asset.analysisTask;
    if (task.stage == VideoAnalysisTaskStage::Summarizing && task.totalFrames > 0) {
        return 90;
    }
    if (task.totalFrames <= 0) {
        return 0;
    }

    const auto completedProgress = 10 + ((task.completedFrames * 75) / qMax(1, task.totalFrames));
    return qBound(0, completedProgress, 90);
}

QString failedAnalysisHint(const GlobalVideoAsset &asset)
{
    const auto &task = asset.analysisTask;
    if (task.stage == VideoAnalysisTaskStage::Summarizing && task.totalFrames > 0) {
        return QStringLiteral("帧图已完成，汇总失败，可继续解析。");
    }
    if (task.totalFrames > 0) {
        return QStringLiteral("解析中断，已完成 %1/%2 帧，可继续解析。")
            .arg(task.completedFrames)
            .arg(task.totalFrames);
    }
    return QStringLiteral("解析失败，可继续解析。");
}

QString readyAnalysisHint(const GlobalVideoAsset &asset)
{
    if (asset.analysisTask.skippedFrames > 0) {
        return QStringLiteral("解析完成，已跳过 %1 帧，可手动补解析。").arg(asset.analysisTask.skippedFrames);
    }
    return QStringLiteral("解析完成，可确认或重新解析。");
}

bool canRetryFrame(const FrameAnalysisRecord &frame)
{
    return frame.analysisState == FrameAnalysisState::Failed || frame.analysisState == FrameAnalysisState::Skipped;
}

QString retryLabel(const FrameAnalysisRecord &frame)
{
    if (frame.retryCount <= 0) {
        return {};
    }
    return QStringLiteral("已重试 %1/%2").arg(frame.retryCount).arg(kMaxFrameRetryCount);
}

bool isSupportedTextAsset(AssetType assetType, const QString &extension)
{
    static const QSet<QString> textExtensions = {
        QStringLiteral("txt"), QStringLiteral("log"), QStringLiteral("md"),
        QStringLiteral("json"), QStringLiteral("csv"), QStringLiteral("tsv"),
        QStringLiteral("xml"), QStringLiteral("yaml"), QStringLiteral("yml"),
        QStringLiteral("docx"), QStringLiteral("xlsx"), QStringLiteral("pptx"),
        QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("vtt")
    };
    const auto normalizedExtension = extension.trimmed().toLower();
    return assetType == AssetType::Subtitle
        || (assetType == AssetType::Document && textExtensions.contains(normalizedExtension));
}

bool canAnalyzeAsset(const GlobalVideoAsset &asset)
{
    return asset.assetType == AssetType::Video
        || asset.assetType == AssetType::Image
        || isSupportedTextAsset(asset.assetType, asset.extension);
}

bool canConfirmAsset(const GlobalVideoAsset &asset)
{
    return asset.analysisStatus == VideoAnalysisStatus::Ready
        && asset.confirmationStatus != ConfirmationStatus::Confirmed;
}

QString previewText(QString text)
{
    text = text.simplified();
    if (text.size() <= 500) {
        return text;
    }
    return text.left(500).trimmed() + QStringLiteral("...");
}

struct SearchUnderstandingTaskResult {
    QString cacheKey;
    QString queryText;
    int generation = 0;
    std::optional<ModelSearchUnderstanding> understanding;
    QString errorMessage;
};

struct FrameRerankTaskResult {
    QString cacheKey;
    QString queryText;
    int generation = 0;
    std::optional<QVector<ModelFrameRerankScore>> scores;
    QString errorMessage;
};
}

MaterialCenterViewModel::MaterialCenterViewModel(MaterialCenterQueryService *queryService,
                                                 MaterialCatalogSyncService *syncService,
                                                 SearchDocumentSyncService *searchDocumentSyncService,
                                                 VideoAnalysisService *analysisService,
                                                 ProjectService *projectService,
                                                 AppSettings *settings,
                                                 VisionApiClient *visionApiClient,
                                                 QObject *parent)
    : QObject(parent)
    , m_queryService(queryService)
    , m_syncService(syncService)
    , m_searchDocumentSyncService(searchDocumentSyncService)
    , m_analysisService(analysisService)
    , m_projectService(projectService)
    , m_settings(settings)
    , m_visionApiClient(visionApiClient)
    , m_model(new MaterialCenterListModel(this))
    , m_folderModel(new MaterialCenterFolderListModel(this))
    , m_frameModel(new MaterialCenterFrameListModel(this))
    , m_detailRefreshTimer(new QTimer(this))
    , m_contactSheetBuildTimer(new QTimer(this))
    , m_searchRefreshTimer(new QTimer(this))
{
    m_detailRefreshTimer->setSingleShot(true);
    m_detailRefreshTimer->setInterval(60);
    connect(m_detailRefreshTimer, &QTimer::timeout, this, &MaterialCenterViewModel::loadPendingDetail);

    m_contactSheetBuildTimer->setSingleShot(true);
    m_contactSheetBuildTimer->setInterval(120);
    connect(m_contactSheetBuildTimer, &QTimer::timeout, this, &MaterialCenterViewModel::buildPendingContactSheet);

    m_searchRefreshTimer->setSingleShot(true);
    m_searchRefreshTimer->setInterval(250);
    connect(m_searchRefreshTimer, &QTimer::timeout, this, &MaterialCenterViewModel::reload);

    if (m_searchDocumentSyncService) {
        connect(m_searchDocumentSyncService,
                &SearchDocumentSyncService::synchronizationProgress,
                this,
                [this](int processed, int total, const QString &detail) {
                    m_semanticIndexing = true;
                    m_semanticIndexProcessed = qMax(0, processed);
                    m_semanticIndexTotal = qMax(0, total);
                    m_semanticIndexStatusText = detail.trimmed().isEmpty()
                        ? QStringLiteral("正在更新语义索引")
                        : detail.trimmed();
                    emit searchStateChanged();
                });
        connect(m_searchDocumentSyncService,
                &SearchDocumentSyncService::synchronizationFinished,
                this,
                [this](bool success,
                       int inserted,
                       int updated,
                       int unchanged,
                       int removed,
                       const QString &message) {
                    m_semanticIndexing = false;
                    if (success) {
                        m_semanticIndexProcessed = m_semanticIndexTotal;
                        m_semanticIndexStatusText = QStringLiteral("语义索引已更新：新增 %1、更新 %2、未变 %3、移除 %4")
                                                        .arg(inserted)
                                                        .arg(updated)
                                                        .arg(unchanged)
                                                        .arg(removed);
                        if (hasActiveSearch()) {
                            m_searchRefreshTimer->start(0);
                        }
                    } else {
                        m_semanticIndexStatusText = QStringLiteral("语义索引更新失败：%1")
                                                        .arg(message.trimmed().isEmpty()
                                                                 ? QStringLiteral("未知错误")
                                                                 : message.trimmed());
                    }
                    emit searchStateChanged();
                });
    }

    if (m_syncService) {
        connect(m_syncService, &MaterialCatalogSyncService::catalogChanged, this, [this]() {
            m_detailCache.clear();
            m_pendingDetailVideoKey.clear();
            m_pendingContactSheetVideoKey.clear();
            m_detailRefreshTimer->stop();
            m_contactSheetBuildTimer->stop();
            reload();
        });
    }
    if (m_analysisService) {
        connect(m_analysisService, &VideoAnalysisService::catalogChanged, this, [this]() {
            m_detailCache.clear();
            m_pendingDetailVideoKey.clear();
            m_pendingContactSheetVideoKey.clear();
            m_detailRefreshTimer->stop();
            m_contactSheetBuildTimer->stop();
            reload();
        });
        connect(m_analysisService, &VideoAnalysisService::analysisProgressChanged, this,
                [this](const QString &videoKey, qint64 progress, const QString &detail, int state, const QString &errorMessage) {
            AnalysisProgressState progressState;
            progressState.progress = progress;
            progressState.detail = detail;
            progressState.errorMessage = errorMessage;
            progressState.state = static_cast<JobState>(state);
            m_analysisProgressByVideoKey.insert(videoKey, progressState);
            if (videoKey == m_detail.asset.videoKey) {
                emit analysisProgressChanged();
                emit dimensionAnalysisChanged();
            }
            if (state == static_cast<int>(JobState::Failed) && !errorMessage.trimmed().isEmpty()) {
                setMessage(errorMessage);
            } else if (state == static_cast<int>(JobState::Completed)) {
                setMessage(detail);
            }
        });
        connect(m_analysisService, &VideoAnalysisService::analysisQueueChanged, this,
                [this](const QString &currentVideoKey, int queuedCount) {
            m_currentAnalysisVideoKey = currentVideoKey;
            m_queuedAnalysisCount = queuedCount;
            emit analysisProgressChanged();
            emit statusChanged();
        });
        connect(m_analysisService, &VideoAnalysisService::dimensionAnalysisProgressChanged, this,
                [this](const QString &videoKey, bool running, const QString &detail, const QString &errorMessage) {
            DimensionProgressState state;
            state.running = running;
            state.detail = detail;
            state.errorMessage = errorMessage;
            m_dimensionProgressByVideoKey.insert(videoKey, state);
            if (videoKey == m_detail.asset.videoKey) {
                emit dimensionAnalysisChanged();
            }
            if (!errorMessage.trimmed().isEmpty()) {
                setMessage(errorMessage);
            } else if (!detail.trimmed().isEmpty()) {
                setMessage(detail);
            }
        });
    }
}

MaterialCenterListModel *MaterialCenterViewModel::model() const
{
    return m_model;
}

MaterialCenterFolderListModel *MaterialCenterViewModel::folderModel() const
{
    return m_folderModel;
}

MaterialCenterFrameListModel *MaterialCenterViewModel::frameModel() const
{
    return m_frameModel;
}

QString MaterialCenterViewModel::statusText() const
{
    int readyCount = 0;
    int pendingConfirmCount = 0;
    for (const auto &asset : m_assets) {
        if (asset.analysisStatus == VideoAnalysisStatus::Ready) {
            ++readyCount;
            if (asset.confirmationStatus == ConfirmationStatus::Pending) {
                ++pendingConfirmCount;
            }
        }
    }
    return QStringLiteral("当前结果 %1 个文件夹 · %2 条素材 · %3 个视觉帧 · 已解析 %4 条 · 待确认 %5 条")
        .arg(m_folders.size())
        .arg(m_assets.size())
        .arg(m_frames.size())
        .arg(readyCount)
        .arg(pendingConfirmCount);
}

QString MaterialCenterViewModel::message() const
{
    return m_message;
}

int MaterialCenterViewModel::folderCount() const
{
    return m_folders.size();
}

int MaterialCenterViewModel::assetCount() const
{
    return m_assets.size();
}

int MaterialCenterViewModel::frameCount() const
{
    return m_frames.size();
}

bool MaterialCenterViewModel::frameSearchMode() const
{
    return hasActiveSearch()
        && m_lastParsedQuery.resultTarget == SearchResultTarget::Frames;
}

bool MaterialCenterViewModel::hasActiveSearch() const
{
    return !m_searchText.trimmed().isEmpty();
}

bool MaterialCenterViewModel::semanticSearchAvailable() const
{
    return m_semanticSearchAvailable;
}

QString MaterialCenterViewModel::semanticSearchStatusText() const
{
    if (!hasActiveSearch()) {
        return {};
    }
    if (m_lastParsedQuery.semanticText.trimmed().isEmpty()) {
        return QStringLiteral("已按日期、类型与结果目标执行结构化检索");
    }
    if (m_semanticIndexing) {
        return QStringLiteral("语义索引正在后台更新，当前搜索不会等待索引");
    }
    return m_semanticSearchAvailable
        ? QStringLiteral("语义检索已启用")
        : QStringLiteral("语义检索不可用，当前使用词法、路径与结构化筛选");
}

bool MaterialCenterViewModel::semanticIndexing() const
{
    return m_semanticIndexing;
}

int MaterialCenterViewModel::semanticIndexProgress() const
{
    if (m_semanticIndexTotal <= 0) {
        return 0;
    }
    return qBound(0,
                  qRound((100.0 * m_semanticIndexProcessed) / m_semanticIndexTotal),
                  100);
}

QString MaterialCenterViewModel::semanticIndexStatusText() const
{
    if (m_semanticIndexing && m_semanticIndexTotal > 0) {
        return QStringLiteral("%1（%2 / %3）")
            .arg(m_semanticIndexStatusText)
            .arg(m_semanticIndexProcessed)
            .arg(m_semanticIndexTotal);
    }
    return m_semanticIndexStatusText;
}

QString MaterialCenterViewModel::searchAssistantStatusText() const
{
    return hasActiveSearch() ? m_searchAssistantStatusText : QString();
}

bool MaterialCenterViewModel::searchAssistantBusy() const
{
    return hasActiveSearch() && m_searchAssistantBusy;
}

bool MaterialCenterViewModel::searchAssistantUsed() const
{
    return hasActiveSearch() && m_searchAssistantUsed;
}

QString MaterialCenterViewModel::searchWarningMessage() const
{
    return m_searchWarningMessage;
}

QString MaterialCenterViewModel::searchInterpretationText() const
{
    return hasActiveSearch() ? m_searchInterpretationText : QString();
}

QString MaterialCenterViewModel::searchEmptyReason() const
{
    return hasActiveSearch() ? m_searchEmptyReason : QString();
}

int MaterialCenterViewModel::excludedPartialCount() const
{
    return m_excludedPartialCount;
}

QVariantList MaterialCenterViewModel::projectOptions() const
{
    return m_projectOptions;
}

QVariantList MaterialCenterViewModel::sourceOptions() const
{
    return m_sourceOptions;
}

QVariantList MaterialCenterViewModel::assetTypeOptions() const
{
    return m_assetTypeOptions;
}

QString MaterialCenterViewModel::projectFilter() const
{
    return m_projectFilter;
}

QString MaterialCenterViewModel::sourceFilter() const
{
    return m_sourceFilter;
}

int MaterialCenterViewModel::assetTypeFilter() const
{
    return m_assetTypeFilter;
}

int MaterialCenterViewModel::analysisStatusFilter() const
{
    return m_analysisStatusFilter;
}

int MaterialCenterViewModel::confirmationStatusFilter() const
{
    return m_confirmationStatusFilter;
}

QVariantList MaterialCenterViewModel::analysisStatusOptions() const
{
    return {
        QVariantMap{{QStringLiteral("value"), -1}, {QStringLiteral("label"), QStringLiteral("全部状态")}},
        QVariantMap{{QStringLiteral("value"), static_cast<int>(VideoAnalysisStatus::Pending)}, {QStringLiteral("label"), QStringLiteral("待解析")}},
        QVariantMap{{QStringLiteral("value"), static_cast<int>(VideoAnalysisStatus::Running)}, {QStringLiteral("label"), QStringLiteral("解析中")}},
        QVariantMap{{QStringLiteral("value"), static_cast<int>(VideoAnalysisStatus::Ready)}, {QStringLiteral("label"), QStringLiteral("已解析")}},
        QVariantMap{{QStringLiteral("value"), static_cast<int>(VideoAnalysisStatus::Failed)}, {QStringLiteral("label"), QStringLiteral("解析失败")}},
        QVariantMap{{QStringLiteral("value"), static_cast<int>(VideoAnalysisStatus::IndexedOnly)}, {QStringLiteral("label"), QStringLiteral("仅索引")}}
    };
}

QVariantList MaterialCenterViewModel::confirmationStatusOptions() const
{
    return {
        QVariantMap{{QStringLiteral("value"), -1}, {QStringLiteral("label"), QStringLiteral("全部确认状态")}},
        QVariantMap{{QStringLiteral("value"), static_cast<int>(ConfirmationStatus::Pending)}, {QStringLiteral("label"), QStringLiteral("未确认")}},
        QVariantMap{{QStringLiteral("value"), static_cast<int>(ConfirmationStatus::Confirmed)}, {QStringLiteral("label"), QStringLiteral("已确认")}}
    };
}

QString MaterialCenterViewModel::selectedVideoKey() const
{
    return m_detail.asset.videoKey;
}

QString MaterialCenterViewModel::selectedAssetKey() const
{
    return m_detail.asset.assetKey.trimmed().isEmpty() ? m_detail.asset.videoKey : m_detail.asset.assetKey;
}

int MaterialCenterViewModel::selectedVideoIndex() const
{
    const auto selectedKey = m_detail.asset.videoKey;
    if (selectedKey.trimmed().isEmpty()) {
        return -1;
    }
    for (int index = 0; index < m_assets.size(); ++index) {
        if (m_assets.at(index).videoKey == selectedKey) {
            return index;
        }
    }
    return -1;
}

bool MaterialCenterViewModel::hasSelection() const
{
    return !m_detail.asset.videoKey.trimmed().isEmpty();
}

QString MaterialCenterViewModel::selectedTitle() const
{
    return m_detail.asset.fileName;
}

QString MaterialCenterViewModel::selectedProjectName() const
{
    return m_detail.asset.projectName;
}

QString MaterialCenterViewModel::selectedSourceName() const
{
    return m_detail.asset.sourceRootName;
}

QString MaterialCenterViewModel::selectedAssetTypeLabel() const
{
    return Formatters::assetTypeLabel(m_detail.asset.assetType);
}

QString MaterialCenterViewModel::selectedExtension() const
{
    return m_detail.asset.extension;
}

QString MaterialCenterViewModel::selectedTechnicalSummary() const
{
    return m_detail.asset.technicalSummary;
}

QString MaterialCenterViewModel::selectedSourceTextPreview() const
{
    return previewText(m_detail.asset.sourceText);
}

QString MaterialCenterViewModel::selectedSummary() const
{
    return m_detail.asset.summary;
}

QVariantList MaterialCenterViewModel::selectedKeywords() const
{
    return stringListToVariants(m_detail.asset.keywords);
}

QVariantList MaterialCenterViewModel::selectedScenes() const
{
    return stringListToVariants(m_detail.asset.scenes);
}

QVariantList MaterialCenterViewModel::selectedDimensionAnalyses() const
{
    return dimensionAnalysesToVariants(m_detail.dimensionAnalyses);
}

QVariantList MaterialCenterViewModel::selectedFrames() const
{
    return m_selectedFramesCache;
}

QString MaterialCenterViewModel::selectedFrameSearchStatus() const
{
    return m_selectedFrameSearchStatusCache;
}

int MaterialCenterViewModel::selectedFrameCount() const
{
    return m_selectedAllFramesCache.size();
}

int MaterialCenterViewModel::selectedVisibleFrameCount() const
{
    return m_selectedFramesCache.size();
}

int MaterialCenterViewModel::selectedRemainingFrameCount() const
{
    return qMax(0, selectedFrameCount() - selectedVisibleFrameCount());
}

bool MaterialCenterViewModel::selectedFramesExpanded() const
{
    return selectedFrameCount() > 0 && selectedVisibleFrameCount() >= selectedFrameCount();
}

bool MaterialCenterViewModel::canExpandSelectedFrames() const
{
    return m_selectedAllFramesCache.size() > kInitialVisibleFrameCount;
}

bool MaterialCenterViewModel::canLoadMoreSelectedFrames() const
{
    return selectedRemainingFrameCount() > 0 && !selectedFramesExpanded();
}

bool MaterialCenterViewModel::selectedFramesLoading() const
{
    return m_selectedFramesLoading;
}

bool MaterialCenterViewModel::selectedThumbnailLoading() const
{
    return hasSelection()
        && m_detail.asset.thumbnailStatus == ThumbnailStatus::Running
        && m_selectedThumbnailUrlCache.isEmpty();
}

QUrl MaterialCenterViewModel::selectedThumbnailUrl() const
{
    return m_selectedThumbnailUrlCache;
}

QString MaterialCenterViewModel::selectedFilePath() const
{
    return m_detail.asset.absolutePath;
}

QString MaterialCenterViewModel::selectedCachePath() const
{
    return hasSelection()
        ? Paths::projectFrameCacheDirectory(m_detail.asset.projectDatabasePath, m_detail.asset.videoKey)
        : QString();
}

QString MaterialCenterViewModel::selectedAnalysisStatusLabel() const
{
    return Formatters::videoAnalysisStatusLabel(m_detail.asset.analysisStatus, m_detail.asset.confirmationStatus);
}

QString MaterialCenterViewModel::selectedConfirmationStatusLabel() const
{
    return Formatters::confirmationStatusLabel(m_detail.asset.confirmationStatus);
}

bool MaterialCenterViewModel::selectedAnalysisBusy() const
{
    const auto state = selectedProgressState().state;
    return state == JobState::Pending || state == JobState::Running;
}

int MaterialCenterViewModel::selectedAnalysisProgress() const
{
    const auto state = selectedProgressState();
    if (!state.detail.trimmed().isEmpty() || state.state == JobState::Pending || state.state == JobState::Running) {
        return static_cast<int>(qBound<qint64>(0LL, state.progress, 100LL));
    }
    return persistedAnalysisProgress(m_detail.asset);
}

QString MaterialCenterViewModel::selectedAnalysisProgressText() const
{
    const auto state = selectedProgressState();
    if (!state.detail.trimmed().isEmpty()) {
        return state.detail;
    }
    if (m_detail.asset.analysisStatus == VideoAnalysisStatus::Failed
        || m_detail.asset.analysisStatus == VideoAnalysisStatus::Running) {
        return failedAnalysisHint(m_detail.asset);
    }
    if (m_detail.asset.analysisStatus == VideoAnalysisStatus::Ready) {
        return readyAnalysisHint(m_detail.asset);
    }
    if (m_detail.asset.analysisStatus == VideoAnalysisStatus::IndexedOnly) {
        return QStringLiteral("该素材类型仅进入索引，不需要内容解析。");
    }
    return hasSelection() ? QStringLiteral("尚未开始解析。") : QString();
}

QString MaterialCenterViewModel::selectedAnalysisError() const
{
    const auto state = selectedProgressState();
    if (!state.errorMessage.trimmed().isEmpty()) {
        return state.errorMessage;
    }
    if (!m_detail.asset.errorMessage.trimmed().isEmpty()) {
        return m_detail.asset.errorMessage;
    }
    return m_detail.asset.analysisStatus == VideoAnalysisStatus::Failed
            || m_detail.asset.analysisStatus == VideoAnalysisStatus::Running
        ? m_detail.asset.analysisTask.lastErrorMessage
        : QString();
}

QString MaterialCenterViewModel::analyzeButtonText() const
{
    const auto state = selectedProgressState().state;
    if (state == JobState::Pending) {
        return QStringLiteral("排队中");
    }
    if (state == JobState::Running) {
        return QStringLiteral("解析中");
    }
    if (!canAnalyzeAsset(m_detail.asset)) {
        return QStringLiteral("仅索引");
    }
    if (m_detail.asset.analysisStatus == VideoAnalysisStatus::Failed
        || m_detail.asset.analysisStatus == VideoAnalysisStatus::Running) {
        return QStringLiteral("继续解析");
    }
    if (m_detail.asset.analysisStatus == VideoAnalysisStatus::Ready) {
        return QStringLiteral("重新解析");
    }
    return QStringLiteral("开始解析");
}

bool MaterialCenterViewModel::canAnalyzeSelected() const
{
    return hasSelection() && canAnalyzeAsset(m_detail.asset) && !selectedAnalysisBusy();
}

bool MaterialCenterViewModel::canConfirmSelected() const
{
    return hasSelection() && canConfirmAsset(m_detail.asset);
}

bool MaterialCenterViewModel::selectedDimensionAnalysisBusy() const
{
    return selectedDimensionProgressState().running;
}

QString MaterialCenterViewModel::selectedDimensionAnalysisText() const
{
    return selectedDimensionProgressState().detail;
}

QString MaterialCenterViewModel::selectedDimensionAnalysisError() const
{
    return selectedDimensionProgressState().errorMessage;
}

bool MaterialCenterViewModel::canAnalyzeSelectedDimensions() const
{
    return hasSelection()
        && m_detail.asset.analysisStatus == VideoAnalysisStatus::Ready
        && !selectedAnalysisBusy()
        && !selectedDimensionAnalysisBusy();
}

bool MaterialCenterViewModel::canAnalyzeVisibleDimensions() const
{
    if (!m_analysisService) {
        return false;
    }

    for (const auto &asset : m_assets) {
        const auto state = m_dimensionProgressByVideoKey.value(asset.videoKey);
        if (canAnalyzeAsset(asset)
            && asset.analysisStatus == VideoAnalysisStatus::Ready
            && !state.running) {
            return true;
        }
    }
    return false;
}

bool MaterialCenterViewModel::selectedIsVideo() const
{
    return hasSelection() && m_detail.asset.assetType == AssetType::Video;
}

int MaterialCenterViewModel::queuedAnalysisCount() const
{
    return m_queuedAnalysisCount;
}

bool MaterialCenterViewModel::canConfirmVisible() const
{
    for (const auto &asset : m_assets) {
        if (canConfirmAsset(asset)) {
            return true;
        }
    }
    return false;
}

bool MaterialCenterViewModel::hasAnalyzedVisible() const
{
    for (const auto &asset : m_assets) {
        const bool hasExistingAnalysis = asset.analysisStatus == VideoAnalysisStatus::Ready
            || asset.analysisStatus == VideoAnalysisStatus::Failed
            || asset.analysisStatus == VideoAnalysisStatus::Running;
        if (hasExistingAnalysis && canAnalyzeAsset(asset)) {
            return true;
        }
    }
    return false;
}

void MaterialCenterViewModel::reload()
{
    if (!m_queryService) {
        return;
    }

    m_searchRefreshTimer->stop();

    m_projectOptions = prependAllOption(m_queryService->fetchProjectOptions(), QStringLiteral("全部项目"));
    m_sourceOptions = prependAllOption(m_queryService->fetchSourceOptions(m_projectFilter), QStringLiteral("全部素材源"));
    m_assetTypeOptions = fileTypeOptions();

    executeSearch();
    startSearchUnderstanding(m_lastParsedQuery);
}

void MaterialCenterViewModel::executeSearch(const ModelSearchUnderstanding *modelUnderstanding)
{
    if (!m_queryService) {
        return;
    }

    MaterialSearchScope scope;
    scope.projectUuid = m_projectFilter;
    scope.sourceRootName = m_sourceFilter;
    scope.analysisStatusFilter = m_analysisStatusFilter;
    scope.confirmationStatusFilter = m_confirmationStatusFilter;
    scope.assetTypeFilter = m_assetTypeFilter;
    const auto result = m_queryService->searchMaterials(m_searchText,
                                                        scope,
                                                        QDate::currentDate(),
                                                        modelUnderstanding);
    applySearchResult(result);
}

void MaterialCenterViewModel::applySearchResult(const MaterialSearchResult &result)
{
    m_folders = result.folders;
    m_assets = result.assets;
    m_frames = result.frames;
    m_semanticSearchAvailable = result.semanticSearchAvailable;
    m_searchWarningMessage = result.warningMessage;
    m_searchInterpretationText = result.parsedQuery.interpretationLabels.join(QStringLiteral(" · "));
    m_lastParsedQuery = result.parsedQuery;
    m_excludedPartialCount = result.excludedPartialCount;
    m_searchEmptyReason.clear();
    if (hasActiveSearch() && m_folders.isEmpty() && m_assets.isEmpty() && m_frames.isEmpty()) {
        if (result.parsedQuery.resultTarget == SearchResultTarget::Folders) {
            m_searchEmptyReason = QStringLiteral("没有找到符合这些条件的文件夹。请检查目录日期、项目范围，或改为搜索素材所在文件夹。");
        } else if (result.parsedQuery.resultTarget == SearchResultTarget::Frames) {
            m_searchEmptyReason = QStringLiteral("没有找到符合条件的视觉帧。请确认素材已完成逐帧视觉解析，或放宽颜色、材质和对象条件。");
        } else {
            m_searchEmptyReason = QStringLiteral("没有找到符合这些条件的素材。请检查拍摄日期和类型；画面内容查询还需要素材完成视觉解析。 ");
        }
        m_searchEmptyReason = m_searchEmptyReason.trimmed();
    }
    m_folderModel->setItems(m_folders);
    m_model->setItems(m_assets);
    m_frameModel->setItems(m_frames);

    QString selectedKey = m_detail.asset.videoKey.trimmed();
    const bool selectedFrameVisible = std::any_of(m_frames.cbegin(),
                                                  m_frames.cend(),
                                                  [&selectedKey](const auto &frame) {
                                                      return frame.videoKey == selectedKey;
                                                  });
    if (!selectedKey.isEmpty()
        && assetByVideoKey(selectedKey).videoKey.trimmed().isEmpty()
        && !selectedFrameVisible) {
        selectedKey.clear();
    }
    if (selectedKey.isEmpty() && !m_assets.isEmpty()) {
        selectedKey = m_assets.first().videoKey;
    }
    prepareSelection(selectedKey);
    emit filtersChanged();
    emit searchStateChanged();
    emit statusChanged();
    emit selectionChanged();
    emit analysisProgressChanged();
    emit dimensionAnalysisChanged();
}

QString MaterialCenterViewModel::searchUnderstandingCacheKey(const QString &queryText,
                                                              const QDate &referenceDate) const
{
    if (!m_settings) {
        return {};
    }
    return QStringLiteral("v1|%1|%2|%3|%4")
        .arg(referenceDate.toString(Qt::ISODate),
             m_settings->visionBaseUrl().toCaseFolded(),
             m_settings->visionModel().toCaseFolded(),
             queryText.simplified().toCaseFolded());
}

void MaterialCenterViewModel::startSearchUnderstanding(const ParsedMaterialQuery &localQuery)
{
    const auto queryText = m_searchText.simplified();
    if (queryText.isEmpty()) {
        m_searchAssistantStatusText.clear();
        m_searchAssistantBusy = false;
        m_searchAssistantUsed = false;
        emit searchStateChanged();
        return;
    }
    if (m_settings && m_settings->localOnlySearch()) {
        m_searchAssistantStatusText = QStringLiteral("仅本地搜索已启用，不会发送查询文字或候选帧");
        m_searchAssistantBusy = false;
        m_searchAssistantUsed = false;
        emit searchStateChanged();
        return;
    }
    if (m_settings && !m_settings->searchAssistantEnabled()) {
        m_searchAssistantStatusText = QStringLiteral("模型查询辅助已关闭，当前使用本地查询理解");
        m_searchAssistantBusy = false;
        m_searchAssistantUsed = false;
        emit searchStateChanged();
        startFrameRerank(localQuery);
        return;
    }
    if (localQuery.semanticText.trimmed().isEmpty()
        && localQuery.strictEntities.isEmpty()
        && localQuery.ocrText.trimmed().isEmpty()) {
        m_searchAssistantStatusText = QStringLiteral("本地规则已完整理解日期、类型与目标，无需调用视觉语言模型");
        m_searchAssistantBusy = false;
        m_searchAssistantUsed = false;
        emit searchStateChanged();
        startFrameRerank(localQuery);
        return;
    }
    if (!m_settings) {
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型未完整配置，已使用本地查询理解");
        m_searchAssistantBusy = false;
        m_searchAssistantUsed = false;
        emit searchStateChanged();
        return;
    }

    const auto referenceDate = QDate::currentDate();
    const auto cacheKey = searchUnderstandingCacheKey(queryText, referenceDate);
    const auto cached = m_searchUnderstandingCache.constFind(cacheKey);
    if (cached != m_searchUnderstandingCache.cend()) {
        bool applied = false;
        SearchQueryUnderstanding::merge(localQuery, cached.value(), &applied);
        if (applied) {
            m_searchAssistantStatusText = QStringLiteral("视觉语言模型已辅助理解（缓存）");
            m_searchAssistantUsed = true;
            executeSearch(&cached.value());
        } else {
            m_searchAssistantStatusText = QStringLiteral("模型未发现需要补充的条件，已使用本地理解");
            m_searchAssistantUsed = false;
        }
        m_searchAssistantBusy = false;
        emit searchStateChanged();
        startFrameRerank(m_lastParsedQuery);
        return;
    }
    if (m_searchUnderstandingInFlight.contains(cacheKey)) {
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型正在理解当前搜索…");
        m_searchAssistantBusy = true;
        emit searchStateChanged();
        return;
    }
    if (!m_visionApiClient
        || m_settings->visionBaseUrl().isEmpty()
        || m_settings->visionApiKey().isEmpty()
        || m_settings->visionModel().isEmpty()) {
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型未完整配置，已使用本地查询理解");
        m_searchAssistantBusy = false;
        m_searchAssistantUsed = false;
        emit searchStateChanged();
        return;
    }
    const auto baseUrl = m_settings->visionBaseUrl();
    const auto apiKey = m_settings->visionApiKey();
    const auto model = m_settings->visionModel();
    const auto timeoutSec = qBound(5, m_settings->analysisTimeoutSec(), 20);
    const auto generation = m_searchGeneration;
    auto *client = m_visionApiClient;
    auto *watcher = new QFutureWatcher<SearchUnderstandingTaskResult>(this);
    m_searchUnderstandingInFlight.insert(cacheKey);
    m_searchAssistantStatusText = QStringLiteral("视觉语言模型正在理解当前搜索…");
    m_searchAssistantBusy = true;
    m_searchAssistantUsed = false;
    emit searchStateChanged();

    connect(watcher, &QFutureWatcher<SearchUnderstandingTaskResult>::finished, this,
            [this, watcher, localQuery]() {
        const auto task = watcher->result();
        watcher->deleteLater();
        m_searchUnderstandingInFlight.remove(task.cacheKey);
        if (task.generation != m_searchGeneration
            || task.queryText != m_searchText.simplified()) {
            m_searchAssistantBusy = !m_searchUnderstandingInFlight.isEmpty()
                || !m_frameRerankInFlight.isEmpty();
            emit searchStateChanged();
            return;
        }
        m_searchAssistantBusy = false;
        if (!task.understanding.has_value()) {
            m_searchAssistantUsed = false;
            m_searchAssistantStatusText = QStringLiteral("模型理解失败，已保留本地搜索：%1")
                .arg(task.errorMessage.isEmpty() ? QStringLiteral("未知错误") : task.errorMessage);
            emit searchStateChanged();
            return;
        }
        if (task.understanding->confidence < 0.55) {
            m_searchAssistantUsed = false;
            m_searchAssistantStatusText = QStringLiteral("模型置信度不足，已使用本地查询理解");
            emit searchStateChanged();
            startFrameRerank(localQuery);
            return;
        }
        bool applied = false;
        SearchQueryUnderstanding::merge(localQuery, *task.understanding, &applied);
        if (!applied) {
            m_searchAssistantUsed = false;
            m_searchAssistantStatusText = QStringLiteral("模型未发现需要补充的条件，已使用本地理解");
            emit searchStateChanged();
            startFrameRerank(localQuery);
            return;
        }
        if (m_searchUnderstandingCache.size() >= 64) {
            m_searchUnderstandingCache.erase(m_searchUnderstandingCache.begin());
        }
        m_searchUnderstandingCache.insert(task.cacheKey, *task.understanding);
        m_searchAssistantUsed = true;
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型已辅助理解");
        executeSearch(&*task.understanding);
        startFrameRerank(m_lastParsedQuery);
    });

    watcher->setFuture(QtConcurrent::run([client,
                                          queryText,
                                          referenceDate,
                                          baseUrl,
                                          apiKey,
                                          model,
                                          timeoutSec,
                                          cacheKey,
                                          generation]() {
        SearchUnderstandingTaskResult task;
        task.cacheKey = cacheKey;
        task.queryText = queryText;
        task.generation = generation;
        task.understanding = client->understandSearchQuery(queryText,
                                                          referenceDate,
                                                          baseUrl,
                                                          apiKey,
                                                          model,
                                                          timeoutSec,
                                                          &task.errorMessage);
        return task;
    }));
}

void MaterialCenterViewModel::startFrameRerank(const ParsedMaterialQuery &query)
{
    if (query.resultTarget != SearchResultTarget::Frames || m_frames.isEmpty()) {
        return;
    }
    if (m_settings && m_settings->localOnlySearch()) {
        m_searchAssistantStatusText = QStringLiteral("仅本地搜索已启用，候选帧保持本地排序");
        m_searchAssistantBusy = false;
        emit searchStateChanged();
        return;
    }
    if (m_settings && !m_settings->frameRerankEnabled()) {
        if (!m_searchAssistantUsed) {
            m_searchAssistantStatusText = QStringLiteral("候选帧视觉复核已关闭，当前使用本地排序");
        }
        m_searchAssistantBusy = false;
        emit searchStateChanged();
        return;
    }
    if (m_settings && !m_settings->allowSearchFrameUpload()) {
        m_searchAssistantStatusText = m_searchAssistantUsed
            ? QStringLiteral("模型已辅助理解；候选帧缩略图发送未授权，保留本地排序")
            : QStringLiteral("候选帧缩略图发送未授权，保留本地排序");
        m_searchAssistantBusy = false;
        emit searchStateChanged();
        return;
    }
    if (!m_settings) {
        return;
    }

    const auto candidateCount = qMin(8, m_frames.size());
    QVector<FrameSearchHit> candidates;
    QStringList candidateKeys;
    candidates.reserve(candidateCount);
    for (int index = 0; index < candidateCount; ++index) {
        candidates.append(m_frames.at(index));
        candidateKeys.append(m_frames.at(index).frameKey);
    }
    const auto cacheKey = QStringLiteral("v1|%1|%2|%3")
        .arg(m_settings->visionModel().toCaseFolded(),
             m_searchText.simplified().toCaseFolded(),
             candidateKeys.join(QLatin1Char('|')));
    const auto cached = m_frameRerankCache.constFind(cacheKey);
    if (cached != m_frameRerankCache.cend()) {
        applyFrameRerank(cached.value());
        m_searchAssistantBusy = false;
        m_searchAssistantUsed = true;
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型已复核候选帧（缓存）");
        emit searchStateChanged();
        return;
    }
    if (m_frameRerankInFlight.contains(cacheKey)) {
        m_searchAssistantBusy = true;
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型正在复核前 %1 个候选帧…").arg(candidateCount);
        emit searchStateChanged();
        return;
    }
    if (!m_visionApiClient
        || m_settings->visionBaseUrl().isEmpty()
        || m_settings->visionApiKey().isEmpty()
        || m_settings->visionModel().isEmpty()) {
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型未完整配置，候选帧保持本地排序");
        m_searchAssistantBusy = false;
        emit searchStateChanged();
        return;
    }
    const auto queryText = m_searchText.simplified();
    const auto generation = m_searchGeneration;
    const auto baseUrl = m_settings->visionBaseUrl();
    const auto apiKey = m_settings->visionApiKey();
    const auto model = m_settings->visionModel();
    const auto timeoutSec = qBound(5, m_settings->analysisTimeoutSec(), 25);
    auto *client = m_visionApiClient;
    auto *watcher = new QFutureWatcher<FrameRerankTaskResult>(this);
    m_frameRerankInFlight.insert(cacheKey);
    m_searchAssistantBusy = true;
    m_searchAssistantStatusText = QStringLiteral("视觉语言模型正在复核前 %1 个候选帧…").arg(candidateCount);
    emit searchStateChanged();

    connect(watcher, &QFutureWatcher<FrameRerankTaskResult>::finished, this, [this, watcher]() {
        const auto task = watcher->result();
        watcher->deleteLater();
        m_frameRerankInFlight.remove(task.cacheKey);
        if (task.generation != m_searchGeneration
            || task.queryText != m_searchText.simplified()) {
            m_searchAssistantBusy = !m_searchUnderstandingInFlight.isEmpty()
                || !m_frameRerankInFlight.isEmpty();
            emit searchStateChanged();
            return;
        }
        m_searchAssistantBusy = false;
        if (!task.scores.has_value()) {
            m_searchAssistantStatusText = QStringLiteral("候选帧视觉复核失败，已保留原排序：%1")
                .arg(task.errorMessage.isEmpty() ? QStringLiteral("未知错误") : task.errorMessage);
            emit searchStateChanged();
            return;
        }
        if (m_frameRerankCache.size() >= 32) {
            m_frameRerankCache.erase(m_frameRerankCache.begin());
        }
        m_frameRerankCache.insert(task.cacheKey, *task.scores);
        applyFrameRerank(*task.scores);
        m_searchAssistantUsed = true;
        m_searchAssistantStatusText = QStringLiteral("视觉语言模型已完成查询理解与候选帧复核");
        emit searchStateChanged();
    });

    watcher->setFuture(QtConcurrent::run([client,
                                          queryText,
                                          candidates,
                                          baseUrl,
                                          apiKey,
                                          model,
                                          timeoutSec,
                                          cacheKey,
                                          generation]() {
        FrameRerankTaskResult task;
        task.cacheKey = cacheKey;
        task.queryText = queryText;
        task.generation = generation;
        task.scores = client->rerankFrameCandidates(queryText,
                                                    candidates,
                                                    baseUrl,
                                                    apiKey,
                                                    model,
                                                    timeoutSec,
                                                    &task.errorMessage);
        return task;
    }));
}

void MaterialCenterViewModel::applyFrameRerank(const QVector<ModelFrameRerankScore> &scores)
{
    QHash<QString, ModelFrameRerankScore> byKey;
    for (const auto &score : scores) {
        byKey.insert(score.frameKey, score);
    }
    for (auto &frame : m_frames) {
        const auto iterator = byKey.constFind(frame.frameKey);
        if (iterator == byKey.cend()) {
            continue;
        }
        frame.score = (frame.score * 0.7) + (iterator->score * 0.3);
        frame.confidence = qBound(0.0,
                                  (frame.confidence * 0.65) + (iterator->score * 0.35),
                                  1.0);
        const auto reason = iterator->reason.trimmed().isEmpty()
            ? QStringLiteral("模型完成画面复核")
            : iterator->reason.trimmed();
        frame.reasons.append(QStringLiteral("视觉模型复核：%1").arg(reason));
        frame.reasons.removeDuplicates();
    }
    std::stable_sort(m_frames.begin(), m_frames.end(), [&byKey](const auto &left, const auto &right) {
        const auto leftIt = byKey.constFind(left.frameKey);
        const auto rightIt = byKey.constFind(right.frameKey);
        const auto category = [](auto iterator, auto end) {
            if (iterator == end) return 1;
            return iterator->relevant ? 0 : 2;
        };
        const auto leftCategory = category(leftIt, byKey.cend());
        const auto rightCategory = category(rightIt, byKey.cend());
        if (leftCategory != rightCategory) {
            return leftCategory < rightCategory;
        }
        if (leftIt != byKey.cend() && rightIt != byKey.cend()
            && !qFuzzyCompare(leftIt->score, rightIt->score)) {
            return leftIt->score > rightIt->score;
        }
        return left.score > right.score;
    });
    m_frameModel->setItems(m_frames);
}

void MaterialCenterViewModel::setSearchText(const QString &searchText)
{
    if (m_searchText == searchText) {
        return;
    }
    m_searchText = searchText;
    ++m_searchGeneration;
    m_searchAssistantStatusText.clear();
    m_searchAssistantBusy = false;
    m_searchAssistantUsed = false;
    m_searchInterpretationText.clear();
    m_searchEmptyReason.clear();
    m_lastParsedQuery = {};
    emit searchStateChanged();
    m_searchRefreshTimer->start();
}

void MaterialCenterViewModel::setProjectFilter(const QString &projectUuid)
{
    if (m_projectFilter == projectUuid) {
        return;
    }
    m_projectFilter = projectUuid;
    m_sourceFilter.clear();
    reload();
}

void MaterialCenterViewModel::setSourceFilter(const QString &sourceName)
{
    if (m_sourceFilter == sourceName) {
        return;
    }
    m_sourceFilter = sourceName;
    reload();
}

void MaterialCenterViewModel::setAssetTypeFilter(int assetType)
{
    if (m_assetTypeFilter == assetType) {
        return;
    }
    m_assetTypeFilter = assetType;
    reload();
}

void MaterialCenterViewModel::setAnalysisStatusFilter(int status)
{
    if (m_analysisStatusFilter == status) {
        return;
    }
    m_analysisStatusFilter = status;
    reload();
}

void MaterialCenterViewModel::setConfirmationStatusFilter(int status)
{
    if (m_confirmationStatusFilter == status) {
        return;
    }
    m_confirmationStatusFilter = status;
    reload();
}

void MaterialCenterViewModel::selectVideo(const QString &videoKey)
{
    const auto normalizedKey = videoKey.trimmed();
    if (m_detail.asset.videoKey == normalizedKey) {
        return;
    }
    prepareSelection(normalizedKey);
    emit selectionChanged();
    emit analysisProgressChanged();
    emit dimensionAnalysisChanged();
}

void MaterialCenterViewModel::selectVideoAt(int index)
{
    if (index < 0 || index >= m_assets.size()) {
        return;
    }
    selectVideo(m_assets.at(index).videoKey);
}

void MaterialCenterViewModel::moveVideoSelection(int delta)
{
    if (m_assets.isEmpty()) {
        return;
    }
    const auto currentIndex = selectedVideoIndex();
    const auto targetIndex = currentIndex < 0
        ? 0
        : qBound(0, currentIndex + delta, m_assets.size() - 1);
    selectVideoAt(targetIndex);
}

void MaterialCenterViewModel::syncCurrentProject()
{
    if (m_syncService) {
        m_syncService->syncCurrentProject();
        setMessage(QStringLiteral("已开始同步当前项目到素材管理中心。"));
    }
}

void MaterialCenterViewModel::rebuildGlobalIndex()
{
    if (m_syncService) {
        m_syncService->rebuildAllProjects();
        setMessage(QStringLiteral("已开始重建全局素材索引。"));
    }
}

void MaterialCenterViewModel::analyzeSelected()
{
    if (!hasSelection() || !m_analysisService) {
        return;
    }
    QString errorMessage;
    if (m_analysisService->enqueueVideo(m_detail.asset.videoKey, &errorMessage)) {
        setMessage(QStringLiteral("已加入解析队列：%1").arg(m_detail.asset.fileName));
    } else {
        setMessage(errorMessage);
        AnalysisProgressState state;
        state.progress = selectedAnalysisProgress();
        state.detail = errorMessage;
        state.errorMessage = errorMessage;
        state.state = JobState::Failed;
        m_analysisProgressByVideoKey.insert(m_detail.asset.videoKey, state);
        emit analysisProgressChanged();
    }
}

void MaterialCenterViewModel::analyzeSelectedDimensions(const QVariantList &dimensions)
{
    if (!hasSelection() || !m_analysisService) {
        return;
    }

    const auto dimensionNames = variantListToStringList(dimensions);
    QString errorMessage;
    if (m_analysisService->analyzeDimensions(m_detail.asset.videoKey, dimensionNames, &errorMessage)) {
        setMessage(QStringLiteral("已开始多维度解析：%1").arg(m_detail.asset.fileName));
        return;
    }

    setMessage(errorMessage);
    DimensionProgressState state;
    state.running = false;
    state.detail = errorMessage;
    state.errorMessage = errorMessage;
    m_dimensionProgressByVideoKey.insert(m_detail.asset.videoKey, state);
    emit dimensionAnalysisChanged();
}

void MaterialCenterViewModel::analyzeVisibleDimensions(const QVariantList &dimensions)
{
    if (!m_analysisService) {
        return;
    }

    const auto dimensionNames = variantListToStringList(dimensions);
    if (dimensionNames.isEmpty()) {
        setMessage(QStringLiteral("请至少添加一个解析维度。"));
        return;
    }

    int accepted = 0;
    int completed = 0;
    QString lastError;
    for (const auto &asset : m_assets) {
        const auto state = m_dimensionProgressByVideoKey.value(asset.videoKey);
        if (!canAnalyzeAsset(asset)
            || asset.analysisStatus != VideoAnalysisStatus::Ready
            || state.running) {
            continue;
        }

        QString pendingError;
        const auto pendingCount = m_analysisService->pendingDimensionCount(asset.videoKey, dimensionNames, &pendingError);
        if (pendingCount == 0) {
            ++completed;
            continue;
        }
        if (pendingCount < 0) {
            if (!pendingError.trimmed().isEmpty()) {
                lastError = pendingError;
            }
            continue;
        }

        QString errorMessage;
        if (m_analysisService->analyzeDimensions(asset.videoKey, dimensionNames, &errorMessage)) {
            ++accepted;
        } else if (!errorMessage.trimmed().isEmpty()) {
            lastError = errorMessage;
        }
    }

    if (accepted > 0) {
        const auto completedText = completed > 0
            ? QStringLiteral("，已跳过 %1 条已完成素材。").arg(completed)
            : QStringLiteral("。");
        setMessage(QStringLiteral("已开始 %1 条素材的全局多维度解析%2").arg(accepted).arg(completedText));
    } else if (completed > 0) {
        setMessage(QStringLiteral("当前结果中所选维度都已解析完成，无需重复解析。"));
    } else if (!lastError.trimmed().isEmpty()) {
        setMessage(lastError);
    } else {
        setMessage(QStringLiteral("当前结果没有可进行多维度解析的已解析素材。"));
    }
    emit dimensionAnalysisChanged();
}

void MaterialCenterViewModel::retrySelectedFrame(int frameNumber)
{
    if (!hasSelection() || !m_analysisService) {
        return;
    }

    QString errorMessage;
    if (m_analysisService->retryFrame(m_detail.asset.videoKey, frameNumber, &errorMessage)) {
        setMessage(QStringLiteral("已加入第 %1 帧重解析队列：%2").arg(frameNumber).arg(m_detail.asset.fileName));
    } else {
        setMessage(errorMessage);
    }
}

void MaterialCenterViewModel::analyzeVisiblePending()
{
    if (!m_analysisService) {
        return;
    }

    QStringList videoKeys;
    for (const auto &asset : m_assets) {
        if (canAnalyzeAsset(asset)
            && (asset.analysisStatus == VideoAnalysisStatus::Pending
            || asset.analysisStatus == VideoAnalysisStatus::Failed
            || asset.analysisStatus == VideoAnalysisStatus::Running)) {
            videoKeys.append(asset.videoKey);
        }
    }
    if (videoKeys.isEmpty()) {
        setMessage(QStringLiteral("当前结果没有未完成素材。"));
        return;
    }

    QString message;
    const auto accepted = m_analysisService->enqueueVideos(videoKeys, &message);
    if (accepted > 0) {
        setMessage(QStringLiteral("已加入 %1 条素材到解析队列。").arg(accepted));
    } else {
        setMessage(message);
    }
}

void MaterialCenterViewModel::analyzeVisibleSupplement()
{
    if (!m_analysisService) {
        return;
    }

    QStringList videoKeys;
    for (const auto &asset : m_assets) {
        if (canAnalyzeAsset(asset)) {
            videoKeys.append(asset.videoKey);
        }
    }
    if (videoKeys.isEmpty()) {
        setMessage(QStringLiteral("当前结果没有可补充解析的素材。"));
        return;
    }

    QString message;
    m_analysisService->enqueueVideosForSupplement(videoKeys, &message);
    setMessage(message.trimmed().isEmpty()
                   ? QStringLiteral("当前结果中没有需要补充解析的素材。")
                   : message);
}

void MaterialCenterViewModel::analyzeVisibleAll()
{
    if (!m_analysisService) {
        return;
    }

    QStringList videoKeys;
    for (const auto &asset : m_assets) {
        if (canAnalyzeAsset(asset)) {
            videoKeys.append(asset.videoKey);
        }
    }
    if (videoKeys.isEmpty()) {
        setMessage(QStringLiteral("当前结果没有可解析的素材。"));
        return;
    }

    QString message;
    m_analysisService->enqueueVideosForRebuild(videoKeys, &message);
    setMessage(message.trimmed().isEmpty()
                   ? QStringLiteral("当前结果没有可重新解析的素材。")
                   : message);
}

void MaterialCenterViewModel::confirmVideo(const QString &videoKey)
{
    if (!m_analysisService) {
        return;
    }

    const auto normalizedKey = videoKey.trimmed();
    if (normalizedKey.isEmpty()) {
        setMessage(QStringLiteral("请先选择一个素材。"));
        return;
    }

    if (m_analysisService->confirmVideo(normalizedKey)) {
        setMessage(QStringLiteral("已确认解析结果，解析图片将永久保留。"));
        reload();
    } else {
        setMessage(QStringLiteral("确认解析结果失败。"));
    }
}

void MaterialCenterViewModel::confirmVisible()
{
    if (!m_analysisService) {
        return;
    }

    QStringList videoKeys;
    for (const auto &asset : m_assets) {
        if (canConfirmAsset(asset)) {
            videoKeys.append(asset.videoKey);
        }
    }

    if (videoKeys.isEmpty()) {
        setMessage(QStringLiteral("当前结果没有待确认素材。"));
        return;
    }

    const auto confirmed = m_analysisService->confirmVideos(videoKeys);
    if (confirmed > 0) {
        setMessage(QStringLiteral("已确认当前结果中的 %1 条素材，解析图片将永久保留。").arg(confirmed));
        reload();
    } else {
        setMessage(QStringLiteral("全部确认失败。"));
    }
}

void MaterialCenterViewModel::confirmSelected()
{
    if (!hasSelection()) {
        return;
    }
    confirmVideo(m_detail.asset.videoKey);
}

bool MaterialCenterViewModel::openSelectedProject()
{
    if (!hasSelection() || !m_projectService) {
        return false;
    }

    if (m_detail.asset.projectDatabasePath.trimmed().isEmpty() && m_queryService) {
        const auto videoKey = m_detail.asset.videoKey;
        auto detail = m_queryService->fetchDetail(videoKey);
        if (!detail.asset.videoKey.trimmed().isEmpty()) {
            m_detailRefreshTimer->stop();
            m_pendingDetailVideoKey.clear();
            m_detail = detail;
            m_detailCache.insert(videoKey, detail);
            refreshSelectedCaches();
            emit selectionChanged();
            emit analysisProgressChanged();
            emit dimensionAnalysisChanged();
        }
    }

    if (m_detail.asset.projectDatabasePath.trimmed().isEmpty()) {
        setMessage(QStringLiteral("无法找到该素材所属项目，不能打开详情。"));
        return false;
    }

    QString errorMessage;
    if (!m_projectService->openProject(m_detail.asset.projectDatabasePath, &errorMessage)) {
        setMessage(errorMessage);
        return false;
    } else {
        setMessage(QStringLiteral("已打开项目：%1").arg(m_detail.asset.projectName));
    }
    return true;
}

void MaterialCenterViewModel::locateSelectedSource()
{
    if (!hasSelection()) {
        return;
    }

    const QFileInfo info(m_detail.asset.absolutePath);
    const auto folder = info.exists() ? info.absolutePath() : QFileInfo(m_detail.asset.absolutePath).absolutePath();
    if (folder.trimmed().isEmpty()) {
        setMessage(QStringLiteral("无法定位素材文件夹。"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void MaterialCenterViewModel::openFolderProject(const QString &folderKey)
{
    if (!m_projectService) {
        return;
    }

    const auto folder = folderByKey(folderKey);
    if (folder.folderKey.isEmpty()) {
        setMessage(QStringLiteral("无法找到该文件夹所属项目。"));
        return;
    }

    QString errorMessage;
    if (!m_projectService->openProject(folder.projectDatabasePath, &errorMessage)) {
        setMessage(errorMessage);
    } else {
        setMessage(QStringLiteral("已打开项目：%1").arg(folder.projectName));
    }
}

void MaterialCenterViewModel::locateFolder(const QString &folderKey)
{
    const auto folder = folderByKey(folderKey);
    const QFileInfo info(folder.absolutePath);
    if (folder.folderKey.isEmpty() || !folder.available || !info.exists() || !info.isDir()) {
        setMessage(QStringLiteral("文件夹当前不可用，无法定位。"));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()))) {
        setMessage(QStringLiteral("无法打开文件夹：%1").arg(info.absoluteFilePath()));
    }
}

void MaterialCenterViewModel::toggleSelectedFramesExpanded()
{
    if (!canExpandSelectedFrames()) {
        return;
    }
    if (selectedFramesExpanded()) {
        collapseSelectedFrames();
    } else {
        showAllSelectedFrames();
    }
}

void MaterialCenterViewModel::loadMoreSelectedFrames()
{
    if (!canLoadMoreSelectedFrames()) {
        return;
    }
    m_selectedVisibleFrameLimit += kVisibleFrameBatchSize;
    applySelectedFrameExpansion();
    emit selectionChanged();
}

void MaterialCenterViewModel::showAllSelectedFrames()
{
    if (selectedFrameCount() <= 0) {
        return;
    }
    m_selectedVisibleFrameLimit = selectedFrameCount();
    applySelectedFrameExpansion();
    emit selectionChanged();
}

void MaterialCenterViewModel::collapseSelectedFrames()
{
    if (selectedFrameCount() <= 0) {
        return;
    }
    m_selectedVisibleFrameLimit = kInitialVisibleFrameCount;
    applySelectedFrameExpansion();
    emit selectionChanged();
}

GlobalVideoAsset MaterialCenterViewModel::assetByVideoKey(const QString &videoKey) const
{
    const auto normalizedKey = videoKey.trimmed();
    for (const auto &asset : m_assets) {
        if (asset.videoKey == normalizedKey) {
            return asset;
        }
    }
    return {};
}

FolderSearchHit MaterialCenterViewModel::folderByKey(const QString &folderKey) const
{
    const auto normalizedKey = folderKey.trimmed();
    for (const auto &folder : m_folders) {
        if (folder.folderKey == normalizedKey) {
            return folder;
        }
    }
    return {};
}

void MaterialCenterViewModel::prepareSelection(const QString &videoKey)
{
    const auto normalizedKey = videoKey.trimmed();

    m_detailRefreshTimer->stop();
    m_contactSheetBuildTimer->stop();
    m_pendingDetailVideoKey.clear();
    m_pendingContactSheetVideoKey.clear();
    m_selectedAllFramesCache.clear();
    m_selectedFramesCache.clear();
    m_selectedFrameSearchStatusCache.clear();
    m_selectedThumbnailUrlCache = {};
    m_selectedVisibleFrameLimit = kInitialVisibleFrameCount;
    m_selectedFramesLoading = false;
    m_detail = {};

    if (normalizedKey.isEmpty()) {
        return;
    }

    const auto asset = assetByVideoKey(normalizedKey);
    if (!asset.videoKey.trimmed().isEmpty()) {
        m_detail.asset = asset;
    } else {
        m_detail.asset.videoKey = normalizedKey;
    }

    if (m_detailCache.contains(normalizedKey)) {
        auto detail = m_detailCache.value(normalizedKey);
        if (!asset.videoKey.trimmed().isEmpty()) {
            detail.asset = asset;
        }
        m_detail = detail;
        refreshSelectedCaches();
        return;
    }

    refreshSelectedThumbnailUrl(false);
    m_selectedFramesLoading = true;
    m_selectedFrameSearchStatusCache = QStringLiteral("正在加载逐帧解析...");
    m_pendingDetailVideoKey = normalizedKey;
    m_detailRefreshTimer->start();
}

void MaterialCenterViewModel::loadPendingDetail()
{
    const auto videoKey = m_pendingDetailVideoKey.trimmed();
    m_pendingDetailVideoKey.clear();

    if (!m_queryService || videoKey.isEmpty()) {
        return;
    }

    auto detail = m_queryService->fetchDetail(videoKey);
    if (detail.asset.videoKey.trimmed().isEmpty()) {
        return;
    }

    const auto asset = assetByVideoKey(videoKey);
    if (!asset.videoKey.trimmed().isEmpty()) {
        detail.asset = asset;
    }
    m_detailCache.insert(videoKey, detail);

    if (m_detail.asset.videoKey != videoKey) {
        return;
    }

    m_detail = detail;
    refreshSelectedCaches();
    emit selectionChanged();
    emit analysisProgressChanged();
    emit dimensionAnalysisChanged();
}

void MaterialCenterViewModel::refreshSelectedCaches()
{
    m_selectedAllFramesCache.clear();
    m_selectedFramesCache.clear();

    if (!hasSelection()) {
        m_selectedVisibleFrameLimit = kInitialVisibleFrameCount;
        m_selectedFramesLoading = false;
        m_selectedFrameSearchStatusCache.clear();
        m_selectedThumbnailUrlCache = {};
        return;
    }

    m_selectedFramesLoading = false;
    auto terms = m_lastParsedQuery.lexicalTerms;
    if (terms.isEmpty() && !m_lastParsedQuery.semanticText.trimmed().isEmpty()) {
        terms = searchTerms(m_lastParsedQuery.semanticText);
    }
    int matchCount = 0;

    for (const auto &frame : m_detail.frames) {
        const bool semanticFrameMatch = m_detail.asset.matchedFrameNumber >= 0
            && frame.frameNumber == m_detail.asset.matchedFrameNumber;
        if (!semanticFrameMatch && !frameMatches(frame, terms)) {
            continue;
        }
        ++matchCount;
        auto matches = matchedFrameTerms(frame, terms);
        if (semanticFrameMatch) {
            matches.prepend(QStringLiteral("视觉语义命中"));
        }
        m_selectedAllFramesCache.append(QVariantMap{
            {QStringLiteral("frameNumber"), frame.frameNumber},
            {QStringLiteral("timestampLabel"), Formatters::formatDuration(frame.timestampMs)},
            {QStringLiteral("imagePath"), frame.imagePath},
            {QStringLiteral("caption"), frame.caption},
            {QStringLiteral("tags"), frame.tags.join(QStringLiteral("、"))},
            {QStringLiteral("objects"), frame.objects.join(QStringLiteral("、"))},
            {QStringLiteral("actions"), frame.actions},
            {QStringLiteral("setting"), frame.setting},
            {QStringLiteral("errorMessage"), frame.errorMessage},
            {QStringLiteral("matchText"), matches.join(QStringLiteral("、"))},
            {QStringLiteral("analysisState"), static_cast<int>(frame.analysisState)},
            {QStringLiteral("retryCount"), frame.retryCount},
            {QStringLiteral("retryLabel"), retryLabel(frame)},
            {QStringLiteral("lastAttemptAt"), frame.lastAttemptAt},
            {QStringLiteral("canRetry"), canRetryFrame(frame)}
        });
    }

    applySelectedFrameExpansion();

    if (terms.isEmpty()) {
        m_selectedFrameSearchStatusCache = QStringLiteral("共 %1 帧解析结果").arg(m_detail.frames.size());
    } else if (matchCount == 0) {
        m_selectedFrameSearchStatusCache = QStringLiteral("搜索“%1”未命中该素材的逐帧解析").arg(m_searchText.trimmed());
    } else {
        m_selectedFrameSearchStatusCache = QStringLiteral("搜索“%1”命中 %2/%3 帧")
            .arg(m_searchText.trimmed())
            .arg(matchCount)
            .arg(m_detail.frames.size());
    }

    refreshSelectedThumbnailUrl(true);
}

void MaterialCenterViewModel::applySelectedFrameExpansion()
{
    m_selectedFramesCache.clear();

    if (m_selectedAllFramesCache.isEmpty()) {
        return;
    }

    const auto visibleCount = qBound(0, m_selectedVisibleFrameLimit, m_selectedAllFramesCache.size());
    if (visibleCount >= m_selectedAllFramesCache.size()) {
        m_selectedFramesCache = m_selectedAllFramesCache;
        return;
    }

    for (int index = 0; index < visibleCount; ++index) {
        m_selectedFramesCache.append(m_selectedAllFramesCache.at(index));
    }
}

void MaterialCenterViewModel::refreshSelectedThumbnailUrl(bool allowContactSheetBuild)
{
    m_selectedThumbnailUrlCache = {};

    if (!hasSelection()) {
        return;
    }

    if (m_detail.asset.assetType == AssetType::Image
        && QFile::exists(m_detail.asset.absolutePath)) {
        m_selectedThumbnailUrlCache = QUrl::fromLocalFile(m_detail.asset.absolutePath);
        return;
    }

    QStringList frameImagePaths;
    frameImagePaths.reserve(m_detail.frames.size());
    for (const auto &frame : m_detail.frames) {
        if (!frame.imagePath.trimmed().isEmpty()) {
            frameImagePaths.append(frame.imagePath);
        }
    }

    if (!frameImagePaths.isEmpty()) {
        const int frameCount = m_settings ? m_settings->contactSheetFrameCount() : 24;
        const auto contactSheetPath = Paths::projectContactSheetPath(m_detail.asset.projectDatabasePath,
                                                                     m_detail.asset.videoKey,
                                                                     frameCount);
        if (QFile::exists(contactSheetPath)) {
            m_selectedThumbnailUrlCache = QUrl::fromLocalFile(contactSheetPath);
            return;
        }

        if (allowContactSheetBuild) {
            m_pendingContactSheetVideoKey = m_detail.asset.videoKey;
            m_contactSheetBuildTimer->start();
        }
    }

    if (!m_detail.asset.thumbnailPath.trimmed().isEmpty()) {
        m_selectedThumbnailUrlCache = QUrl::fromLocalFile(m_detail.asset.thumbnailPath);
    }
}

void MaterialCenterViewModel::buildPendingContactSheet()
{
    const auto videoKey = m_pendingContactSheetVideoKey.trimmed();
    m_pendingContactSheetVideoKey.clear();

    if (videoKey.isEmpty()) {
        return;
    }

    VideoAnalysisDetail detail;
    if (m_detail.asset.videoKey == videoKey) {
        detail = m_detail;
    } else if (m_detailCache.contains(videoKey)) {
        detail = m_detailCache.value(videoKey);
    } else {
        return;
    }

    QStringList frameImagePaths;
    frameImagePaths.reserve(detail.frames.size());
    for (const auto &frame : detail.frames) {
        if (!frame.imagePath.trimmed().isEmpty()) {
            frameImagePaths.append(frame.imagePath);
        }
    }
    if (frameImagePaths.isEmpty()) {
        return;
    }

    const int frameCount = m_settings ? m_settings->contactSheetFrameCount() : 24;
    const auto contactSheetPath = Paths::projectContactSheetPath(detail.asset.projectDatabasePath,
                                                                 detail.asset.videoKey,
                                                                 frameCount);
    if (!QFile::exists(contactSheetPath)) {
        QString errorMessage;
        if (!ContactSheetBuilder::build(frameImagePaths, frameCount, contactSheetPath, &errorMessage)) {
            return;
        }
    }

    if (m_detail.asset.videoKey == videoKey && QFile::exists(contactSheetPath)) {
        m_selectedThumbnailUrlCache = QUrl::fromLocalFile(contactSheetPath);
        emit selectionChanged();
    }
}

void MaterialCenterViewModel::refreshDetail()
{
    if (!m_queryService || m_detail.asset.videoKey.trimmed().isEmpty()) {
        return;
    }
    prepareSelection(m_detail.asset.videoKey);
}

void MaterialCenterViewModel::setMessage(const QString &message)
{
    if (m_message == message) {
        return;
    }
    m_message = message;
    emit statusChanged();
}

MaterialCenterViewModel::AnalysisProgressState MaterialCenterViewModel::selectedProgressState() const
{
    const auto key = m_detail.asset.videoKey;
    if (!key.trimmed().isEmpty() && m_analysisProgressByVideoKey.contains(key)) {
        return m_analysisProgressByVideoKey.value(key);
    }
    return {};
}

MaterialCenterViewModel::DimensionProgressState MaterialCenterViewModel::selectedDimensionProgressState() const
{
    const auto key = m_detail.asset.videoKey;
    if (!key.trimmed().isEmpty() && m_dimensionProgressByVideoKey.contains(key)) {
        return m_dimensionProgressByVideoKey.value(key);
    }
    return {};
}

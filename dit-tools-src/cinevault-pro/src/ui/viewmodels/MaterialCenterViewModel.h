#pragma once

#include "domain/Entities.h"
#include "domain/SearchTypes.h"

#include <QHash>
#include <QDate>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

class MaterialCatalogSyncService;
class MaterialCenterFolderListModel;
class MaterialCenterFrameListModel;
class MaterialCenterListModel;
class MaterialCenterQueryService;
class ProjectService;
class SearchDocumentSyncService;
class VideoAnalysisService;
class AppSettings;
class VisionApiClient;

class MaterialCenterViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(MaterialCenterListModel* model READ model CONSTANT)
    Q_PROPERTY(MaterialCenterListModel* assetModel READ model CONSTANT)
    Q_PROPERTY(MaterialCenterFolderListModel* folderModel READ folderModel CONSTANT)
    Q_PROPERTY(MaterialCenterFrameListModel* frameModel READ frameModel CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString message READ message NOTIFY statusChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY searchStateChanged)
    Q_PROPERTY(int assetCount READ assetCount NOTIFY searchStateChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY searchStateChanged)
    Q_PROPERTY(bool frameSearchMode READ frameSearchMode NOTIFY searchStateChanged)
    Q_PROPERTY(bool hasActiveSearch READ hasActiveSearch NOTIFY searchStateChanged)
    Q_PROPERTY(bool semanticSearchAvailable READ semanticSearchAvailable NOTIFY searchStateChanged)
    Q_PROPERTY(QString semanticSearchStatusText READ semanticSearchStatusText NOTIFY searchStateChanged)
    Q_PROPERTY(bool semanticIndexing READ semanticIndexing NOTIFY searchStateChanged)
    Q_PROPERTY(int semanticIndexProgress READ semanticIndexProgress NOTIFY searchStateChanged)
    Q_PROPERTY(QString semanticIndexStatusText READ semanticIndexStatusText NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchAssistantStatusText READ searchAssistantStatusText NOTIFY searchStateChanged)
    Q_PROPERTY(bool searchAssistantBusy READ searchAssistantBusy NOTIFY searchStateChanged)
    Q_PROPERTY(bool searchAssistantUsed READ searchAssistantUsed NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchWarningMessage READ searchWarningMessage NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchInterpretationText READ searchInterpretationText NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchEmptyReason READ searchEmptyReason NOTIFY searchStateChanged)
    Q_PROPERTY(int excludedPartialCount READ excludedPartialCount NOTIFY searchStateChanged)
    Q_PROPERTY(QVariantList projectOptions READ projectOptions NOTIFY filtersChanged)
    Q_PROPERTY(QVariantList sourceOptions READ sourceOptions NOTIFY filtersChanged)
    Q_PROPERTY(QVariantList assetTypeOptions READ assetTypeOptions NOTIFY filtersChanged)
    Q_PROPERTY(QString projectFilter READ projectFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString sourceFilter READ sourceFilter NOTIFY filtersChanged)
    Q_PROPERTY(int assetTypeFilter READ assetTypeFilter NOTIFY filtersChanged)
    Q_PROPERTY(int analysisStatusFilter READ analysisStatusFilter NOTIFY filtersChanged)
    Q_PROPERTY(int confirmationStatusFilter READ confirmationStatusFilter NOTIFY filtersChanged)
    Q_PROPERTY(QVariantList analysisStatusOptions READ analysisStatusOptions CONSTANT)
    Q_PROPERTY(QVariantList confirmationStatusOptions READ confirmationStatusOptions CONSTANT)
    Q_PROPERTY(QString selectedVideoKey READ selectedVideoKey NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAssetKey READ selectedAssetKey NOTIFY selectionChanged)
    Q_PROPERTY(int selectedVideoIndex READ selectedVideoIndex NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTitle READ selectedTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedProjectName READ selectedProjectName NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSourceName READ selectedSourceName NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAssetTypeLabel READ selectedAssetTypeLabel NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedExtension READ selectedExtension NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTechnicalSummary READ selectedTechnicalSummary NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSourceTextPreview READ selectedSourceTextPreview NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSummary READ selectedSummary NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedKeywords READ selectedKeywords NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedScenes READ selectedScenes NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedDimensionAnalyses READ selectedDimensionAnalyses NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedFrames READ selectedFrames NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFrameSearchStatus READ selectedFrameSearchStatus NOTIFY selectionChanged)
    Q_PROPERTY(int selectedFrameCount READ selectedFrameCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedVisibleFrameCount READ selectedVisibleFrameCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRemainingFrameCount READ selectedRemainingFrameCount NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedFramesExpanded READ selectedFramesExpanded NOTIFY selectionChanged)
    Q_PROPERTY(bool canExpandSelectedFrames READ canExpandSelectedFrames NOTIFY selectionChanged)
    Q_PROPERTY(bool canLoadMoreSelectedFrames READ canLoadMoreSelectedFrames NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedFramesLoading READ selectedFramesLoading NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedThumbnailLoading READ selectedThumbnailLoading NOTIFY selectionChanged)
    Q_PROPERTY(QUrl selectedThumbnailUrl READ selectedThumbnailUrl NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFilePath READ selectedFilePath NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCachePath READ selectedCachePath NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAnalysisStatusLabel READ selectedAnalysisStatusLabel NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedConfirmationStatusLabel READ selectedConfirmationStatusLabel NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedAnalysisBusy READ selectedAnalysisBusy NOTIFY analysisProgressChanged)
    Q_PROPERTY(int selectedAnalysisProgress READ selectedAnalysisProgress NOTIFY analysisProgressChanged)
    Q_PROPERTY(QString selectedAnalysisProgressText READ selectedAnalysisProgressText NOTIFY analysisProgressChanged)
    Q_PROPERTY(QString selectedAnalysisError READ selectedAnalysisError NOTIFY analysisProgressChanged)
    Q_PROPERTY(QString analyzeButtonText READ analyzeButtonText NOTIFY analysisProgressChanged)
    Q_PROPERTY(bool canAnalyzeSelected READ canAnalyzeSelected NOTIFY analysisProgressChanged)
    Q_PROPERTY(bool canConfirmSelected READ canConfirmSelected NOTIFY analysisProgressChanged)
    Q_PROPERTY(bool selectedDimensionAnalysisBusy READ selectedDimensionAnalysisBusy NOTIFY dimensionAnalysisChanged)
    Q_PROPERTY(QString selectedDimensionAnalysisText READ selectedDimensionAnalysisText NOTIFY dimensionAnalysisChanged)
    Q_PROPERTY(QString selectedDimensionAnalysisError READ selectedDimensionAnalysisError NOTIFY dimensionAnalysisChanged)
    Q_PROPERTY(bool canAnalyzeSelectedDimensions READ canAnalyzeSelectedDimensions NOTIFY dimensionAnalysisChanged)
    Q_PROPERTY(bool canAnalyzeVisibleDimensions READ canAnalyzeVisibleDimensions NOTIFY dimensionAnalysisChanged)
    Q_PROPERTY(bool selectedIsVideo READ selectedIsVideo NOTIFY selectionChanged)
    Q_PROPERTY(int queuedAnalysisCount READ queuedAnalysisCount NOTIFY analysisProgressChanged)
    Q_PROPERTY(bool canConfirmVisible READ canConfirmVisible NOTIFY statusChanged)
    Q_PROPERTY(bool hasAnalyzedVisible READ hasAnalyzedVisible NOTIFY statusChanged)

public:
    explicit MaterialCenterViewModel(MaterialCenterQueryService *queryService,
                                     MaterialCatalogSyncService *syncService,
                                     SearchDocumentSyncService *searchDocumentSyncService,
                                     VideoAnalysisService *analysisService,
                                     ProjectService *projectService,
                                     AppSettings *settings,
                                     VisionApiClient *visionApiClient,
                                     QObject *parent = nullptr);

    MaterialCenterListModel *model() const;
    MaterialCenterFolderListModel *folderModel() const;
    MaterialCenterFrameListModel *frameModel() const;
    QString statusText() const;
    QString message() const;
    int folderCount() const;
    int assetCount() const;
    int frameCount() const;
    bool frameSearchMode() const;
    bool hasActiveSearch() const;
    bool semanticSearchAvailable() const;
    QString semanticSearchStatusText() const;
    bool semanticIndexing() const;
    int semanticIndexProgress() const;
    QString semanticIndexStatusText() const;
    QString searchAssistantStatusText() const;
    bool searchAssistantBusy() const;
    bool searchAssistantUsed() const;
    QString searchWarningMessage() const;
    QString searchInterpretationText() const;
    QString searchEmptyReason() const;
    int excludedPartialCount() const;
    QVariantList projectOptions() const;
    QVariantList sourceOptions() const;
    QVariantList assetTypeOptions() const;
    QString projectFilter() const;
    QString sourceFilter() const;
    int assetTypeFilter() const;
    int analysisStatusFilter() const;
    int confirmationStatusFilter() const;
    QVariantList analysisStatusOptions() const;
    QVariantList confirmationStatusOptions() const;
    QString selectedVideoKey() const;
    QString selectedAssetKey() const;
    int selectedVideoIndex() const;
    bool hasSelection() const;
    QString selectedTitle() const;
    QString selectedProjectName() const;
    QString selectedSourceName() const;
    QString selectedAssetTypeLabel() const;
    QString selectedExtension() const;
    QString selectedTechnicalSummary() const;
    QString selectedSourceTextPreview() const;
    QString selectedSummary() const;
    QVariantList selectedKeywords() const;
    QVariantList selectedScenes() const;
    QVariantList selectedDimensionAnalyses() const;
    QVariantList selectedFrames() const;
    QString selectedFrameSearchStatus() const;
    int selectedFrameCount() const;
    int selectedVisibleFrameCount() const;
    int selectedRemainingFrameCount() const;
    bool selectedFramesExpanded() const;
    bool canExpandSelectedFrames() const;
    bool canLoadMoreSelectedFrames() const;
    bool selectedFramesLoading() const;
    bool selectedThumbnailLoading() const;
    QUrl selectedThumbnailUrl() const;
    QString selectedFilePath() const;
    QString selectedCachePath() const;
    QString selectedAnalysisStatusLabel() const;
    QString selectedConfirmationStatusLabel() const;
    bool selectedAnalysisBusy() const;
    int selectedAnalysisProgress() const;
    QString selectedAnalysisProgressText() const;
    QString selectedAnalysisError() const;
    QString analyzeButtonText() const;
    bool canAnalyzeSelected() const;
    bool canConfirmSelected() const;
    bool selectedDimensionAnalysisBusy() const;
    QString selectedDimensionAnalysisText() const;
    QString selectedDimensionAnalysisError() const;
    bool canAnalyzeSelectedDimensions() const;
    bool canAnalyzeVisibleDimensions() const;
    bool selectedIsVideo() const;
    int queuedAnalysisCount() const;
    bool canConfirmVisible() const;
    bool hasAnalyzedVisible() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setSearchText(const QString &searchText);
    Q_INVOKABLE void setProjectFilter(const QString &projectUuid);
    Q_INVOKABLE void setSourceFilter(const QString &sourceName);
    Q_INVOKABLE void setAssetTypeFilter(int assetType);
    Q_INVOKABLE void setAnalysisStatusFilter(int status);
    Q_INVOKABLE void setConfirmationStatusFilter(int status);
    Q_INVOKABLE void selectVideo(const QString &videoKey);
    Q_INVOKABLE void selectVideoAt(int index);
    Q_INVOKABLE void moveVideoSelection(int delta);
    Q_INVOKABLE void syncCurrentProject();
    Q_INVOKABLE void rebuildGlobalIndex();
    Q_INVOKABLE void analyzeSelected();
    Q_INVOKABLE void analyzeSelectedDimensions(const QVariantList &dimensions);
    Q_INVOKABLE void analyzeVisibleDimensions(const QVariantList &dimensions);
    Q_INVOKABLE void analyzeVisiblePending();
    Q_INVOKABLE void analyzeVisibleSupplement();
    Q_INVOKABLE void analyzeVisibleAll();
    Q_INVOKABLE void confirmVideo(const QString &videoKey);
    Q_INVOKABLE void confirmVisible();
    Q_INVOKABLE void confirmSelected();
    Q_INVOKABLE bool openSelectedProject();
    Q_INVOKABLE void locateSelectedSource();
    Q_INVOKABLE void openFolderProject(const QString &folderKey);
    Q_INVOKABLE void locateFolder(const QString &folderKey);
    Q_INVOKABLE void toggleSelectedFramesExpanded();
    Q_INVOKABLE void loadMoreSelectedFrames();
    Q_INVOKABLE void showAllSelectedFrames();
    Q_INVOKABLE void collapseSelectedFrames();
    Q_INVOKABLE void retrySelectedFrame(int frameNumber);

signals:
    void statusChanged();
    void filtersChanged();
    void searchStateChanged();
    void selectionChanged();
    void analysisProgressChanged();
    void dimensionAnalysisChanged();

private:
    struct AnalysisProgressState {
        qint64 progress = 0;
        QString detail;
        QString errorMessage;
        JobState state = JobState::Completed;
    };

    struct DimensionProgressState {
        bool running = false;
        QString detail;
        QString errorMessage;
    };

    GlobalVideoAsset assetByVideoKey(const QString &videoKey) const;
    FolderSearchHit folderByKey(const QString &folderKey) const;
    void prepareSelection(const QString &videoKey);
    void loadPendingDetail();
    void refreshSelectedCaches();
    void applySelectedFrameExpansion();
    void refreshSelectedThumbnailUrl(bool allowContactSheetBuild);
    void buildPendingContactSheet();
    void refreshDetail();
    void setMessage(const QString &message);
    void executeSearch(const ModelSearchUnderstanding *modelUnderstanding = nullptr);
    void applySearchResult(const MaterialSearchResult &result);
    void startSearchUnderstanding(const ParsedMaterialQuery &localQuery);
    void startFrameRerank(const ParsedMaterialQuery &query);
    void applyFrameRerank(const QVector<ModelFrameRerankScore> &scores);
    QString searchUnderstandingCacheKey(const QString &queryText, const QDate &referenceDate) const;
    AnalysisProgressState selectedProgressState() const;
    DimensionProgressState selectedDimensionProgressState() const;

    MaterialCenterQueryService *m_queryService = nullptr;
    MaterialCatalogSyncService *m_syncService = nullptr;
    SearchDocumentSyncService *m_searchDocumentSyncService = nullptr;
    VideoAnalysisService *m_analysisService = nullptr;
    ProjectService *m_projectService = nullptr;
    AppSettings *m_settings = nullptr;
    VisionApiClient *m_visionApiClient = nullptr;
    MaterialCenterListModel *m_model = nullptr;
    MaterialCenterFolderListModel *m_folderModel = nullptr;
    MaterialCenterFrameListModel *m_frameModel = nullptr;
    QVector<FolderSearchHit> m_folders;
    QVector<GlobalVideoAsset> m_assets;
    QVector<FrameSearchHit> m_frames;
    VideoAnalysisDetail m_detail;
    QString m_searchText;
    ParsedMaterialQuery m_lastParsedQuery;
    QString m_projectFilter;
    QString m_sourceFilter;
    int m_assetTypeFilter = -1;
    int m_analysisStatusFilter = -1;
    int m_confirmationStatusFilter = -1;
    QVariantList m_projectOptions;
    QVariantList m_sourceOptions;
    QVariantList m_assetTypeOptions;
    QString m_message;
    bool m_semanticSearchAvailable = false;
    bool m_semanticIndexing = false;
    int m_semanticIndexProcessed = 0;
    int m_semanticIndexTotal = 0;
    QString m_semanticIndexStatusText;
    QString m_searchWarningMessage;
    QString m_searchInterpretationText;
    QString m_searchEmptyReason;
    int m_excludedPartialCount = 0;
    QString m_searchAssistantStatusText;
    bool m_searchAssistantBusy = false;
    bool m_searchAssistantUsed = false;
    int m_searchGeneration = 0;
    QHash<QString, ModelSearchUnderstanding> m_searchUnderstandingCache;
    QSet<QString> m_searchUnderstandingInFlight;
    QHash<QString, QVector<ModelFrameRerankScore>> m_frameRerankCache;
    QSet<QString> m_frameRerankInFlight;
    QHash<QString, AnalysisProgressState> m_analysisProgressByVideoKey;
    QHash<QString, DimensionProgressState> m_dimensionProgressByVideoKey;
    QHash<QString, VideoAnalysisDetail> m_detailCache;
    QVariantList m_selectedAllFramesCache;
    QVariantList m_selectedFramesCache;
    QString m_selectedFrameSearchStatusCache;
    QUrl m_selectedThumbnailUrlCache;
    QString m_pendingDetailVideoKey;
    QString m_pendingContactSheetVideoKey;
    QTimer *m_detailRefreshTimer = nullptr;
    QTimer *m_contactSheetBuildTimer = nullptr;
    QTimer *m_searchRefreshTimer = nullptr;
    int m_selectedVisibleFrameLimit = 24;
    bool m_selectedFramesLoading = false;
    QString m_currentAnalysisVideoKey;
    int m_queuedAnalysisCount = 0;
};

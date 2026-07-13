#pragma once

#include <QObject>
#include <QVariant>

class QQmlApplicationEngine;
class WindowThemeController;
class LocalImageUrlHelper;
#if CINEVAULT_BUILD_MINIMAL_GUI
class MinimalShellViewModel;
class MinimalSourceRailViewModel;
class MinimalLibraryWorkspaceViewModel;
class MinimalInspectorViewModel;
class MinimalJobTimelineViewModel;
class MinimalReportWorkspaceViewModel;
#else
#include "infrastructure/config/AppSettings.h"
class DatabaseManager;
class GlobalDatabaseManager;
class SearchEngine;
class FFmpegAdapter;
class JobEngine;
class MediaProbeEngine;
class ScanEngine;
class ThumbnailEngine;
class ProjectService;
class JobService;
class MediaTaskService;
class MaterialCatalogSyncService;
class MaterialCenterQueryService;
class VideoAnalysisService;
class FeedbackService;
class UpdateService;
class ImportService;
class LibraryQueryService;
class DocumentPreviewService;
class ReportExportService;
class VisionApiClient;
class ShellViewModel;
class ProjectLibraryViewModel;
class SourceRailViewModel;
class LibraryWorkspaceViewModel;
class MaterialCenterViewModel;
class InspectorViewModel;
class JobTimelineViewModel;
class ReportWorkspaceViewModel;
class SettingsViewModel;
class FeedbackViewModel;
#endif

class AppContext : public QObject {
    Q_OBJECT

public:
    explicit AppContext(QObject *parent = nullptr);

    void expose(QQmlApplicationEngine &engine);

private:
    WindowThemeController *m_windowThemeController = nullptr;
    LocalImageUrlHelper *m_localImageUrlHelper = nullptr;

#if CINEVAULT_BUILD_MINIMAL_GUI
    MinimalShellViewModel *m_shellViewModel = nullptr;
    MinimalSourceRailViewModel *m_sourceRailViewModel = nullptr;
    MinimalLibraryWorkspaceViewModel *m_libraryWorkspaceViewModel = nullptr;
    MinimalInspectorViewModel *m_inspectorViewModel = nullptr;
    MinimalJobTimelineViewModel *m_jobTimelineViewModel = nullptr;
    MinimalReportWorkspaceViewModel *m_reportWorkspaceViewModel = nullptr;
#else
    AppSettings m_settings;
    DatabaseManager *m_databaseManager = nullptr;
    GlobalDatabaseManager *m_globalDatabaseManager = nullptr;
    SearchEngine *m_searchEngine = nullptr;
    FFmpegAdapter *m_ffmpegAdapter = nullptr;
    JobEngine *m_jobEngine = nullptr;
    MediaProbeEngine *m_mediaProbeEngine = nullptr;
    ScanEngine *m_scanEngine = nullptr;
    ThumbnailEngine *m_thumbnailEngine = nullptr;
    ProjectService *m_projectService = nullptr;
    JobService *m_jobService = nullptr;
    MediaTaskService *m_mediaTaskService = nullptr;
    MaterialCatalogSyncService *m_materialCatalogSyncService = nullptr;
    MaterialCenterQueryService *m_materialCenterQueryService = nullptr;
    VisionApiClient *m_visionApiClient = nullptr;
    VideoAnalysisService *m_videoAnalysisService = nullptr;
    FeedbackService *m_feedbackService = nullptr;
    UpdateService *m_updateService = nullptr;
    ImportService *m_importService = nullptr;
    LibraryQueryService *m_libraryQueryService = nullptr;
    DocumentPreviewService *m_documentPreviewService = nullptr;
    ReportExportService *m_reportExportService = nullptr;
    ShellViewModel *m_shellViewModel = nullptr;
    ProjectLibraryViewModel *m_projectLibraryViewModel = nullptr;
    SourceRailViewModel *m_sourceRailViewModel = nullptr;
    LibraryWorkspaceViewModel *m_libraryWorkspaceViewModel = nullptr;
    MaterialCenterViewModel *m_materialCenterViewModel = nullptr;
    InspectorViewModel *m_inspectorViewModel = nullptr;
    JobTimelineViewModel *m_jobTimelineViewModel = nullptr;
    ReportWorkspaceViewModel *m_reportWorkspaceViewModel = nullptr;
    SettingsViewModel *m_settingsViewModel = nullptr;
    FeedbackViewModel *m_feedbackViewModel = nullptr;
#endif
};

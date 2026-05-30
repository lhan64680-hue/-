#include "app/AppContext.h"

#if CINEVAULT_BUILD_MINIMAL_GUI
#include "ui/viewmodels/MinimalImportWorkspaceViewModel.h"
#include "ui/viewmodels/MinimalInspectorViewModel.h"
#include "ui/viewmodels/MinimalJobTimelineViewModel.h"
#include "ui/viewmodels/MinimalLibraryWorkspaceViewModel.h"
#include "ui/viewmodels/MinimalShellViewModel.h"
#include "ui/viewmodels/MinimalSourceRailViewModel.h"
#else
#include "application/ImportService.h"
#include "application/JobService.h"
#include "application/LibraryQueryService.h"
#include "application/ProjectService.h"
#include "core/media/MediaProbeEngine.h"
#include "core/jobs/JobEngine.h"
#include "core/scan/ScanEngine.h"
#include "core/search/SearchEngine.h"
#include "core/thumbnail/ThumbnailEngine.h"
#include "infrastructure/db/DatabaseManager.h"
#include "infrastructure/ffmpeg/FFmpegAdapter.h"
#include "ui/viewmodels/ImportWorkspaceViewModel.h"
#include "ui/viewmodels/InspectorViewModel.h"
#include "ui/viewmodels/JobTimelineViewModel.h"
#include "ui/viewmodels/LibraryWorkspaceViewModel.h"
#include "ui/viewmodels/ShellViewModel.h"
#include "ui/viewmodels/SourceRailViewModel.h"
#endif

#include <QQmlApplicationEngine>
#include <QQmlContext>

AppContext::AppContext(QObject *parent)
    : QObject(parent)
#if CINEVAULT_BUILD_MINIMAL_GUI
    , m_shellViewModel(new MinimalShellViewModel(this))
    , m_sourceRailViewModel(new MinimalSourceRailViewModel(this))
    , m_importWorkspaceViewModel(new MinimalImportWorkspaceViewModel(this))
    , m_libraryWorkspaceViewModel(new MinimalLibraryWorkspaceViewModel(this))
    , m_inspectorViewModel(new MinimalInspectorViewModel(m_sourceRailViewModel, m_libraryWorkspaceViewModel, this))
    , m_jobTimelineViewModel(new MinimalJobTimelineViewModel(this))
{
    connect(m_shellViewModel, &MinimalShellViewModel::searchRequested, m_libraryWorkspaceViewModel, &MinimalLibraryWorkspaceViewModel::setSearchText);
    connect(m_sourceRailViewModel, &MinimalSourceRailViewModel::sourceSelected, m_libraryWorkspaceViewModel, &MinimalLibraryWorkspaceViewModel::setSourceFilter);
    connect(m_sourceRailViewModel, &MinimalSourceRailViewModel::sourceSelected, m_inspectorViewModel, &MinimalInspectorViewModel::showSource);
    connect(m_libraryWorkspaceViewModel, &MinimalLibraryWorkspaceViewModel::assetSelected, m_inspectorViewModel, &MinimalInspectorViewModel::showAsset);
}
#else
    , m_databaseManager(new DatabaseManager(this))
    , m_searchEngine(new SearchEngine)
    , m_ffmpegAdapter(new FFmpegAdapter)
    , m_jobEngine(new JobEngine(m_databaseManager, this))
    , m_mediaProbeEngine(new MediaProbeEngine(m_ffmpegAdapter, this))
    , m_thumbnailEngine(new ThumbnailEngine(m_ffmpegAdapter, this))
    , m_scanEngine(new ScanEngine(m_databaseManager, m_jobEngine, m_mediaProbeEngine, m_thumbnailEngine, this))
    , m_projectService(new ProjectService(m_databaseManager, &m_settings, this))
    , m_jobService(new JobService(m_jobEngine, this))
    , m_importService(new ImportService(m_databaseManager, m_jobService, m_scanEngine, this))
    , m_libraryQueryService(new LibraryQueryService(m_databaseManager, m_searchEngine, this))
    , m_shellViewModel(new ShellViewModel(m_projectService, m_importService, this))
    , m_sourceRailViewModel(new SourceRailViewModel(m_libraryQueryService, this))
    , m_importWorkspaceViewModel(new ImportWorkspaceViewModel(m_importService, this))
    , m_libraryWorkspaceViewModel(new LibraryWorkspaceViewModel(m_libraryQueryService, this))
    , m_inspectorViewModel(new InspectorViewModel(m_libraryQueryService, this))
    , m_jobTimelineViewModel(new JobTimelineViewModel(m_jobService, this))
{
    connect(m_projectService, &ProjectService::projectChanged, m_sourceRailViewModel, &SourceRailViewModel::reload);
    connect(m_projectService, &ProjectService::projectChanged, m_libraryWorkspaceViewModel, &LibraryWorkspaceViewModel::reload);
    connect(m_projectService, &ProjectService::projectChanged, m_jobTimelineViewModel, &JobTimelineViewModel::reload);
    connect(m_projectService, &ProjectService::projectChanged, m_inspectorViewModel, &InspectorViewModel::clear);

    connect(m_importService, &ImportService::catalogChanged, m_sourceRailViewModel, &SourceRailViewModel::reload);
    connect(m_importService, &ImportService::catalogChanged, m_libraryWorkspaceViewModel, &LibraryWorkspaceViewModel::reload);

    connect(m_shellViewModel, &ShellViewModel::searchRequested, m_libraryWorkspaceViewModel, &LibraryWorkspaceViewModel::setSearchText);
    connect(m_sourceRailViewModel, &SourceRailViewModel::sourceSelected, m_libraryWorkspaceViewModel, &LibraryWorkspaceViewModel::setSourceFilter);
    connect(m_sourceRailViewModel, &SourceRailViewModel::sourceSelected, m_inspectorViewModel, &InspectorViewModel::showSource);
    connect(m_libraryWorkspaceViewModel, &LibraryWorkspaceViewModel::assetSelected, m_inspectorViewModel, &InspectorViewModel::showAsset);
}
#endif

void AppContext::expose(QQmlApplicationEngine &engine)
{
    auto *context = engine.rootContext();
    context->setContextProperty(QStringLiteral("shellVm"), m_shellViewModel);
    context->setContextProperty(QStringLiteral("sourceRailVm"), m_sourceRailViewModel);
    context->setContextProperty(QStringLiteral("importWorkspaceVm"), m_importWorkspaceViewModel);
    context->setContextProperty(QStringLiteral("libraryWorkspaceVm"), m_libraryWorkspaceViewModel);
    context->setContextProperty(QStringLiteral("inspectorVm"), m_inspectorViewModel);
    context->setContextProperty(QStringLiteral("jobTimelineVm"), m_jobTimelineViewModel);
}

#pragma once

#include <QObject>

#include "infrastructure/config/AppSettings.h"

class QQmlApplicationEngine;
class DatabaseManager;
class SearchEngine;
class JobEngine;
class ScanEngine;
class ProjectService;
class JobService;
class ImportService;
class LibraryQueryService;
class ShellViewModel;
class SourceRailViewModel;
class ImportWorkspaceViewModel;
class LibraryWorkspaceViewModel;
class InspectorViewModel;
class JobTimelineViewModel;

class AppContext : public QObject {
    Q_OBJECT

public:
    explicit AppContext(QObject *parent = nullptr);

    void expose(QQmlApplicationEngine &engine);

private:
    AppSettings m_settings;
    DatabaseManager *m_databaseManager = nullptr;
    SearchEngine *m_searchEngine = nullptr;
    JobEngine *m_jobEngine = nullptr;
    ScanEngine *m_scanEngine = nullptr;
    ProjectService *m_projectService = nullptr;
    JobService *m_jobService = nullptr;
    ImportService *m_importService = nullptr;
    LibraryQueryService *m_libraryQueryService = nullptr;
    ShellViewModel *m_shellViewModel = nullptr;
    SourceRailViewModel *m_sourceRailViewModel = nullptr;
    ImportWorkspaceViewModel *m_importWorkspaceViewModel = nullptr;
    LibraryWorkspaceViewModel *m_libraryWorkspaceViewModel = nullptr;
    InspectorViewModel *m_inspectorViewModel = nullptr;
    JobTimelineViewModel *m_jobTimelineViewModel = nullptr;
};

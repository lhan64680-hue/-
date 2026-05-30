cmake_minimum_required(VERSION 3.24)

project(CineVault VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(DEFINED QT_ROOT AND EXISTS "${QT_ROOT}")
    list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Sql
    Concurrent
    Widgets
)

qt_standard_project_setup(REQUIRES 6.5)

set(APP_SOURCES
    src/app/main.cpp
    src/app/AppBootstrap.h
    src/app/AppBootstrap.cpp
    src/app/AppContext.h
    src/app/AppContext.cpp
    src/domain/Enums.h
    src/domain/Entities.h
    src/shared/Paths.h
    src/shared/Paths.cpp
    src/shared/Formatters.h
    src/shared/Formatters.cpp
    src/infrastructure/config/AppSettings.h
    src/infrastructure/config/AppSettings.cpp
    src/infrastructure/logging/Logger.h
    src/infrastructure/logging/Logger.cpp
    src/infrastructure/db/DatabaseManager.h
    src/infrastructure/db/DatabaseManager.cpp
    src/core/search/SearchEngine.h
    src/core/search/SearchEngine.cpp
    src/core/scan/FileTypeService.h
    src/core/scan/FileTypeService.cpp
    src/core/jobs/JobEngine.h
    src/core/jobs/JobEngine.cpp
    src/core/scan/ScanEngine.h
    src/core/scan/ScanEngine.cpp
    src/application/ProjectService.h
    src/application/ProjectService.cpp
    src/application/JobService.h
    src/application/JobService.cpp
    src/application/ImportService.h
    src/application/ImportService.cpp
    src/application/LibraryQueryService.h
    src/application/LibraryQueryService.cpp
    src/ui/models/SourceRootListModel.h
    src/ui/models/SourceRootListModel.cpp
    src/ui/models/AssetListModel.h
    src/ui/models/AssetListModel.cpp
    src/ui/models/JobListModel.h
    src/ui/models/JobListModel.cpp
    src/ui/viewmodels/ShellViewModel.h
    src/ui/viewmodels/ShellViewModel.cpp
    src/ui/viewmodels/SourceRailViewModel.h
    src/ui/viewmodels/SourceRailViewModel.cpp
    src/ui/viewmodels/ImportWorkspaceViewModel.h
    src/ui/viewmodels/ImportWorkspaceViewModel.cpp
    src/ui/viewmodels/LibraryWorkspaceViewModel.h
    src/ui/viewmodels/LibraryWorkspaceViewModel.cpp
    src/ui/viewmodels/InspectorViewModel.h
    src/ui/viewmodels/InspectorViewModel.cpp
    src/ui/viewmodels/JobTimelineViewModel.h
    src/ui/viewmodels/JobTimelineViewModel.cpp
)

qt_add_executable(CineVault
    ${APP_SOURCES}
)

set_source_files_properties(src/ui/qml/themes/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(CineVault
    URI CineVault
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/themes/Theme.qml
        src/ui/qml/components/TopCommandBar.qml
        src/ui/qml/components/SourceRail.qml
        src/ui/qml/components/InspectorPane.qml
        src/ui/qml/components/JobTimelineBar.qml
        src/ui/qml/components/SourceCard.qml
        src/ui/qml/components/StatusChip.qml
        src/ui/qml/components/AssetCard.qml
        src/ui/qml/workspaces/ImportWorkspace.qml
        src/ui/qml/workspaces/LibraryWorkspace.qml
        src/ui/qml/workspaces/QcWorkspace.qml
        src/ui/qml/workspaces/ReportWorkspace.qml
        src/ui/qml/workspaces/JobsWorkspace.qml
)

target_include_directories(CineVault PRIVATE src)

target_link_libraries(CineVault PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Widgets
)

if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(CineVaultSmokeTest tests/unit/SmokeTest.cpp)
    target_include_directories(CineVaultSmokeTest PRIVATE src)
    target_link_libraries(CineVaultSmokeTest PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME CineVaultSmokeTest COMMAND CineVaultSmokeTest)
endif()

install(TARGETS CineVault
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)

qt_finalize_executable(CineVault)

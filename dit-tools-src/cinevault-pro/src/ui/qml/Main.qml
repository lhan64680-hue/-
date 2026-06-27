import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Effects
import QtQuick.Layouts
import CineVault

ApplicationWindow {
    id: root

    property var shellViewModel: shellVm
    property var projectLibraryViewModel: projectLibraryVm
    property var materialBackupViewModel: materialBackupVm
    property var sourceRailViewModel: sourceRailVm
    property var libraryViewModel: libraryWorkspaceVm
    property var materialCenterViewModel: materialCenterVm
    property var documentPreviewViewModel: documentPreviewVm
    property var inspectorViewModel: inspectorVm
    property var jobTimelineViewModel: jobTimelineVm
    property var reportViewModel: reportWorkspaceVm
    property var settingsViewModel: settingsVm
    readonly property bool hasOpenProject: root.shellViewModel && root.shellViewModel.projectEntered
    readonly property bool isProjectLibraryWorkspace: root.shellViewModel && root.shellViewModel.currentWorkspace === root.shellViewModel.projectLibraryWorkspaceId
    property bool projectLibraryWorkspaceLoaded: true
    property bool materialBackupWorkspaceLoaded: false
    property bool libraryWorkspaceLoaded: false
    property bool materialCenterWorkspaceLoaded: false
    property bool reportWorkspaceLoaded: false
    property bool jobsWorkspaceLoaded: false
    property bool sourceRailCollapsed: false

    visible: true
    width: 1600
    height: 980
    minimumWidth: 1280
    minimumHeight: 760
    title: "影资管家"
    color: Theme.bg

    function applyWindowTheme() {
        if (windowThemeController) {
            windowThemeController.apply(root, Theme.topBar, Theme.text, Theme.isDark)
        }
    }

    function rememberWorkspace(workspace) {
        if (!root.shellViewModel) {
            return
        }
        if (workspace === root.shellViewModel.projectLibraryWorkspaceId) {
            root.projectLibraryWorkspaceLoaded = true
        } else if (workspace === root.shellViewModel.materialBackupWorkspaceId) {
            root.materialBackupWorkspaceLoaded = true
        } else if (workspace === root.shellViewModel.materialCenterWorkspaceId) {
            root.materialCenterWorkspaceLoaded = true
        } else if (workspace === root.shellViewModel.reportWorkspaceId) {
            root.reportWorkspaceLoaded = true
        } else if (workspace === root.shellViewModel.jobsWorkspaceId) {
            root.jobsWorkspaceLoaded = true
        } else {
            root.libraryWorkspaceLoaded = true
        }
    }

    Component.onCompleted: {
        root.rememberWorkspace(root.shellViewModel.currentWorkspace)
        Qt.callLater(root.applyWindowTheme)
    }
    onVisibleChanged: if (visible) Qt.callLater(root.applyWindowTheme)

    Binding {
        target: Theme
        property: "mode"
        value: root.settingsViewModel ? root.settingsViewModel.themeMode : Theme.modeSystem
        restoreMode: Binding.RestoreBinding
    }

    Connections {
        target: Theme
        function onIsDarkChanged() { root.applyWindowTheme() }
        function onTopBarChanged() { root.applyWindowTheme() }
        function onTextChanged() { root.applyWindowTheme() }
    }

    Connections {
        target: root.shellViewModel
        function onCurrentWorkspaceChanged() {
            root.rememberWorkspace(root.shellViewModel.currentWorkspace)
        }
    }

    FolderDialog {
        id: sourceFolderDialog
        title: "选择素材源目录"
        onAccepted: root.shellViewModel.importSourceDirectory(selectedFolder)
        onRejected: root.shellViewModel.cancelAddSourceDirectory()
    }

    Connections {
        target: root.shellViewModel
        function onAddSourceDirectoryRequested() {
            sourceFolderDialog.open()
        }
        function onOpenSettingsRequested() {
            settingsDialog.open()
        }
    }

    SettingsDialog {
        id: settingsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        viewModel: root.settingsViewModel
    }

    Item {
        id: appContentLayer

        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TopCommandBar {
                Layout.fillWidth: true
                shellVm: root.shellViewModel
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                SourceRail {
                    Layout.preferredWidth: root.shellViewModel.currentWorkspace === root.shellViewModel.materialCenterWorkspaceId
                        || root.shellViewModel.currentWorkspace === root.shellViewModel.materialBackupWorkspaceId
                        || root.isProjectLibraryWorkspace
                        || !root.hasOpenProject
                        ? 0
                        : (root.sourceRailCollapsed ? 56 : 270)
                    Layout.fillHeight: true
                    visible: root.shellViewModel.currentWorkspace !== root.shellViewModel.materialCenterWorkspaceId
                        && root.shellViewModel.currentWorkspace !== root.shellViewModel.materialBackupWorkspaceId
                        && !root.isProjectLibraryWorkspace
                        && root.hasOpenProject
                    shellVm: root.shellViewModel
                    viewModel: root.sourceRailViewModel
                    collapsed: root.sourceRailCollapsed
                    onCollapseRequested: function(nextCollapsed) {
                        root.sourceRailCollapsed = nextCollapsed
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Loader {
                        anchors.fill: parent
                        active: root.projectLibraryWorkspaceLoaded
                        visible: root.shellViewModel.currentWorkspace === root.shellViewModel.projectLibraryWorkspaceId
                        asynchronous: true
                        sourceComponent: projectLibraryWorkspaceComponent
                    }

                    Loader {
                        anchors.fill: parent
                        active: root.materialBackupWorkspaceLoaded
                        visible: root.shellViewModel.currentWorkspace === root.shellViewModel.materialBackupWorkspaceId
                        asynchronous: true
                        sourceComponent: materialBackupWorkspaceComponent
                    }

                    Loader {
                        anchors.fill: parent
                        active: root.libraryWorkspaceLoaded
                        visible: root.shellViewModel.currentWorkspace !== root.shellViewModel.projectLibraryWorkspaceId
                            && root.shellViewModel.currentWorkspace !== root.shellViewModel.materialBackupWorkspaceId
                            && root.shellViewModel.currentWorkspace !== root.shellViewModel.materialCenterWorkspaceId
                            && root.shellViewModel.currentWorkspace !== root.shellViewModel.reportWorkspaceId
                            && root.shellViewModel.currentWorkspace !== root.shellViewModel.jobsWorkspaceId
                        asynchronous: true
                        sourceComponent: libraryWorkspaceComponent
                    }

                    Loader {
                        anchors.fill: parent
                        active: root.materialCenterWorkspaceLoaded
                        visible: root.shellViewModel.currentWorkspace === root.shellViewModel.materialCenterWorkspaceId
                        asynchronous: true
                        sourceComponent: materialCenterWorkspaceComponent
                    }

                    Loader {
                        anchors.fill: parent
                        active: root.reportWorkspaceLoaded
                        visible: root.shellViewModel.currentWorkspace === root.shellViewModel.reportWorkspaceId
                        asynchronous: true
                        sourceComponent: reportWorkspaceComponent
                    }

                    Loader {
                        anchors.fill: parent
                        active: root.jobsWorkspaceLoaded
                        visible: root.shellViewModel.currentWorkspace === root.shellViewModel.jobsWorkspaceId
                        asynchronous: true
                        sourceComponent: jobsWorkspaceComponent
                    }
                }

                InspectorPane {
                    Layout.preferredWidth: root.shellViewModel.currentWorkspace === root.shellViewModel.reportWorkspaceId
                        || root.shellViewModel.currentWorkspace === root.shellViewModel.materialCenterWorkspaceId
                        || root.shellViewModel.currentWorkspace === root.shellViewModel.materialBackupWorkspaceId
                        || root.isProjectLibraryWorkspace
                        || !root.hasOpenProject
                        ? 0
                        : 330
                    Layout.fillHeight: true
                    visible: root.shellViewModel.currentWorkspace !== root.shellViewModel.reportWorkspaceId
                        && root.shellViewModel.currentWorkspace !== root.shellViewModel.materialCenterWorkspaceId
                        && root.shellViewModel.currentWorkspace !== root.shellViewModel.materialBackupWorkspaceId
                        && !root.isProjectLibraryWorkspace
                        && root.hasOpenProject
                    viewModel: root.inspectorViewModel
                    mediaViewModel: root.libraryViewModel
                }
            }

            JobTimelineBar {
                Layout.fillWidth: true
                Layout.preferredHeight: root.isProjectLibraryWorkspace || root.shellViewModel.currentWorkspace === root.shellViewModel.materialBackupWorkspaceId ? 0 : implicitHeight
                visible: !root.isProjectLibraryWorkspace && root.shellViewModel.currentWorkspace !== root.shellViewModel.materialBackupWorkspaceId
                viewModel: root.jobTimelineViewModel
                libraryViewModel: root.libraryViewModel
            }
        }
    }

    MultiEffect {
        anchors.fill: appContentLayer
        source: appContentLayer
        visible: settingsDialog.opened
        blurEnabled: true
        blurMax: 32
        blur: 0.72
        z: 800
    }

    Rectangle {
        anchors.fill: parent
        visible: settingsDialog.opened
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.36 : 0.28)
        z: 801
    }

    Component {
        id: projectLibraryWorkspaceComponent
        ProjectLibraryWorkspace {
            viewModel: root.projectLibraryViewModel
        }
    }

    Component {
        id: materialBackupWorkspaceComponent
        MaterialBackupWorkspace {
            viewModel: root.materialBackupViewModel
        }
    }

    Component {
        id: libraryWorkspaceComponent
        LibraryWorkspace {
            viewModel: root.libraryViewModel
            shellVm: root.shellViewModel
            documentPreviewVm: root.documentPreviewViewModel
        }
    }

    Component {
        id: reportWorkspaceComponent
        ReportWorkspace {
            viewModel: root.reportViewModel
        }
    }

    Component {
        id: materialCenterWorkspaceComponent
        MaterialCenterWorkspace {
            viewModel: root.materialCenterViewModel
            shellVm: root.shellViewModel
        }
    }

    Component {
        id: jobsWorkspaceComponent
            JobsWorkspace {
            viewModel: root.jobTimelineViewModel
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

ApplicationWindow {
    visible: true
    width: 1600
    height: 980
    minimumWidth: 1280
    minimumHeight: 760
    title: "影资管家"
    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopCommandBar {
            Layout.fillWidth: true
            shellVm: shellVm
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            SourceRail {
                Layout.preferredWidth: 290
                Layout.fillHeight: true
                viewModel: sourceRailVm
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: {
                    switch (shellVm.currentWorkspace) {
                    case 0: return importWorkspaceComponent
                    case 1: return libraryWorkspaceComponent
                    case 2: return qcWorkspaceComponent
                    case 3: return reportWorkspaceComponent
                    case 4: return jobsWorkspaceComponent
                    default: return importWorkspaceComponent
                    }
                }
            }

            InspectorPane {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                viewModel: inspectorVm
            }
        }

        JobTimelineBar {
            Layout.fillWidth: true
            viewModel: jobTimelineVm
        }
    }

    Component {
        id: importWorkspaceComponent
        ImportWorkspace {
            shellVm: shellVm
            viewModel: importWorkspaceVm
        }
    }

    Component {
        id: libraryWorkspaceComponent
        LibraryWorkspace {
            viewModel: libraryWorkspaceVm
        }
    }

    Component {
        id: qcWorkspaceComponent
        QcWorkspace {
            viewModel: libraryWorkspaceVm
        }
    }

    Component {
        id: reportWorkspaceComponent
        ReportWorkspace {}
    }

    Component {
        id: jobsWorkspaceComponent
        JobsWorkspace {
            viewModel: jobTimelineVm
        }
    }
}

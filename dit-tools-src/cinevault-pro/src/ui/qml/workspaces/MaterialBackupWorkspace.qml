import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    id: root

    property var viewModel

    color: Theme.bg

    component EmptyState: Rectangle {
        property string label: ""

        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 8
        color: Theme.panel2
        border.width: 1
        border.color: Theme.line

        Text {
            anchors.centerIn: parent
            width: parent.width - 36
            text: label
            color: Theme.muted
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    component GlowProgressBar: ProgressBar {
        id: progressBar

        property bool glowActive: false

        background: Rectangle {
            implicitHeight: 10
            radius: 5
            color: Theme.panel2
            border.width: 1
            border.color: progressBar.glowActive ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.45) : Theme.line
        }

        contentItem: Item {
            implicitHeight: 10
            clip: true

            Rectangle {
                id: progressFill
                width: progressBar.indeterminate ? parent.width : progressBar.visualPosition * parent.width
                height: parent.height
                radius: height / 2
                color: progressBar.indeterminate
                       ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.42)
                       : Theme.blue
                clip: true

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: progressBar.glowActive ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                }

                Item {
                    id: progressGlow
                    width: Math.max(54, progressFill.width * 0.36)
                    height: parent.height
                    visible: progressBar.glowActive && progressFill.width > 8
                    opacity: 0.9

                    Rectangle {
                        x: parent.width * 0.12
                        width: parent.width * 0.76
                        height: parent.height
                        radius: height / 2
                        color: Qt.rgba(1, 1, 1, 0.18)
                    }

                    Rectangle {
                        x: parent.width * 0.36
                        width: parent.width * 0.28
                        height: parent.height
                        radius: height / 2
                        color: Qt.rgba(1, 1, 1, 0.38)
                    }

                    SequentialAnimation on x {
                        running: progressGlow.visible
                        loops: Animation.Infinite
                        NumberAnimation {
                            from: -progressGlow.width
                            to: progressFill.width
                            duration: 1450
                            easing.type: Easing.InOutSine
                        }
                        PauseAnimation { duration: 220 }
                    }
                }
            }

            Rectangle {
                id: emptyTrackGlow
                width: Math.max(42, parent.width * 0.18)
                height: parent.height
                radius: height / 2
                visible: progressBar.glowActive && progressFill.width <= 8
                color: Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.25)

                SequentialAnimation on x {
                    running: emptyTrackGlow.visible
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: -emptyTrackGlow.width
                        to: emptyTrackGlow.parent.width
                        duration: 1450
                        easing.type: Easing.InOutSine
                    }
                    PauseAnimation { duration: 220 }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            radius: 8
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: "待备份源"
                    color: Theme.text
                    font.pixelSize: 22
                    font.weight: Font.Black
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ActionButton {
                        Layout.fillWidth: true
                        text: "添加文件"
                        enabled: viewModel && !viewModel.running
                        onClicked: viewModel.addFileSources()
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        text: "添加文件夹"
                        enabled: viewModel && !viewModel.running
                        onClicked: viewModel.addFolderSource()
                    }
                }

                ActionButton {
                    Layout.fillWidth: true
                    text: "添加磁盘卷"
                    enabled: viewModel && !viewModel.running
                    onClicked: viewModel.addVolumeSource()
                }

                ListView {
                    id: sourceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    model: viewModel ? viewModel.sourceModel : null
                    visible: count > 0
                    ScrollBar.vertical: ThemedScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 116
                        radius: 8
                        color: Theme.panel2
                        border.width: 1
                        border.color: Theme.line

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                StatusChip {
                                    label: model.kindLabel
                                    tint: Theme.blue
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.path
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.fileCount + " 个文件 · " + model.sizeText
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: model.statusText
                                    color: model.readable ? Theme.green : Theme.orange
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                ActionButton {
                                    Layout.preferredWidth: 54
                                    Layout.preferredHeight: 28
                                    text: "移除"
                                    danger: true
                                    textPixelSize: 12
                                    enabled: viewModel && !viewModel.running
                                    onClicked: viewModel.removeSource(index)
                                }
                            }
                        }
                    }
                }

                EmptyState {
                    label: "尚未添加源"
                    visible: sourceList.count === 0
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 420
            Layout.fillHeight: true
            radius: 8
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: "备份目的地"
                    color: Theme.text
                    font.pixelSize: 22
                    font.weight: Font.Black
                    elide: Text.ElideRight
                }

                ActionButton {
                    Layout.fillWidth: true
                    text: "添加目的地"
                    enabled: viewModel && !viewModel.running
                    onClicked: viewModel.addDestination()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    CheckBox {
                        id: cascadeToggle
                        enabled: viewModel && !viewModel.running
                        checked: viewModel ? viewModel.cascadeEnabled : false
                        onToggled: if (viewModel) viewModel.cascadeEnabled = checked
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "级联拷贝"
                        color: Theme.text
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }

                    ThemedComboBox {
                        Layout.preferredWidth: 132
                        model: viewModel ? viewModel.verificationOptions : []
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: viewModel ? viewModel.verificationMode : 0
                        enabled: viewModel && !viewModel.running
                        onActivated: if (viewModel) viewModel.verificationMode = currentValue
                    }
                }

                ListView {
                    id: destinationList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    model: viewModel ? viewModel.destinationModel : null
                    visible: count > 0
                    ScrollBar.vertical: ThemedScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 128
                        radius: 8
                        color: model.primary ? Theme.selectedBg : Theme.panel2
                        border.width: 1
                        border.color: model.primary ? Theme.selectedLine : Theme.line

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                StatusChip {
                                    label: model.primary ? "主目标" : "副本"
                                    tint: model.primary ? Theme.green : Theme.blue
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.rootPath
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "批次路径：" + model.plannedRootPath
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: model.availableText + " · " + model.statusText
                                    color: model.writable ? Theme.green : Theme.orange
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                ActionButton {
                                    Layout.preferredWidth: 54
                                    Layout.preferredHeight: 28
                                    text: "主目标"
                                    textPixelSize: 12
                                    enabled: viewModel && !viewModel.running && !model.primary
                                    onClicked: viewModel.setPrimaryDestination(index)
                                }

                                ActionButton {
                                    Layout.preferredWidth: 54
                                    Layout.preferredHeight: 28
                                    text: "移除"
                                    danger: true
                                    textPixelSize: 12
                                    enabled: viewModel && !viewModel.running
                                    onClicked: viewModel.removeDestination(index)
                                }
                            }
                        }
                    }
                }

                EmptyState {
                    label: "尚未添加目的地"
                    visible: destinationList.count === 0
                }

                ActionButton {
                    Layout.fillWidth: true
                    text: "添加到备份任务"
                    enabled: viewModel && viewModel.canAddBackupTask
                    onClicked: viewModel.enqueueBackupTask()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: "拷贝任务"
                    color: Theme.text
                    font.pixelSize: 22
                    font.weight: Font.Black
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: viewModel ? viewModel.summaryText : ""
                    color: Theme.muted
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }

                GlowProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: viewModel ? viewModel.overallProgress : 0
                    glowActive: viewModel && viewModel.running
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    model: viewModel ? viewModel.taskModel : null
                    visible: count > 0
                    ScrollBar.vertical: ThemedScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: model.errorMessage.length > 0 ? 130 : 104
                        radius: 8
                        color: Theme.panel2
                        border.width: 1
                        border.color: model.primary ? Theme.selectedLine : Theme.line

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                StatusChip {
                                    label: model.stateLabel
                                    tint: (model.state === 5 || model.state === 6) ? Theme.red : (model.state === 4 ? Theme.orange : (model.state === 3 ? Theme.green : Theme.blue))
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.plannedRootPath
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                            }

                            GlowProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: model.progress
                                glowActive: viewModel && viewModel.running && (model.state === 1 || model.state === 2)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: model.copiedText + " · " + model.speedText
                                    color: Theme.muted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.preferredWidth: 180
                                    text: model.statusText
                                    color: Theme.muted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignRight
                                }

                                ActionButton {
                                    Layout.preferredWidth: 54
                                    Layout.preferredHeight: 28
                                    visible: model.state === 0 && viewModel && !viewModel.running
                                    text: "移除"
                                    danger: true
                                    textPixelSize: 12
                                    onClicked: viewModel.removeQueuedTask(model.destinationId)
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: model.errorMessage.length > 0
                                text: model.errorMessage
                                color: Theme.red
                                font.pixelSize: 12
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: viewModel && viewModel.lastMessage.length > 0
                    text: viewModel ? viewModel.lastMessage : ""
                    color: Theme.muted
                    font.pixelSize: 12
                    maximumLineCount: 3
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ActionButton {
                        Layout.fillWidth: true
                        text: "开始备份"
                        primary: true
                        enabled: viewModel && viewModel.canStartBackup
                        onClicked: viewModel.startBackup()
                    }

                    ActionButton {
                        Layout.preferredWidth: 96
                        text: "取消"
                        danger: true
                        enabled: viewModel && viewModel.running
                        onClicked: viewModel.cancelBackup()
                    }
                }
            }
        }
    }
}

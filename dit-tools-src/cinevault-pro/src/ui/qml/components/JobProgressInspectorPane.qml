import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var viewModel
    readonly property bool hasCurrentItem: viewModel && viewModel.hasActiveBatchItem
    readonly property int currentProgress: viewModel ? viewModel.batchCurrentProgress : 0
    readonly property int batchProgress: viewModel ? viewModel.batchProgress : 0

    function metricValue(value) {
        return viewModel ? String(value) : "0"
    }

    color: Theme.panel
    border.width: 1
    border.color: Theme.line

    component StatTile: Rectangle {
        property string label
        property string value
        property color valueColor: Theme.text

        Layout.fillWidth: true
        Layout.preferredHeight: 64
        radius: 12
        color: Theme.panel2
        border.width: 1
        border.color: Theme.line

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 4

            Text {
                width: parent.width
                text: label
                color: Theme.muted
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: value
                color: valueColor
                font.pixelSize: 17
                font.weight: Font.Black
                elide: Text.ElideRight
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: "处理进度"
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.Bold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: hasCurrentItem ? viewModel.batchCurrentLabel : "暂无正在处理的素材"
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 210
            radius: 16
            color: Theme.mediaSurface
            border.width: 1
            border.color: Theme.line
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Column {
                        anchors.centerIn: parent
                        width: parent.width
                        spacing: 7

                        Text {
                            width: parent.width
                            text: hasCurrentItem ? viewModel.batchCurrentLabel : "等待视频解析任务"
                            color: Theme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: hasCurrentItem ? viewModel.batchCurrentDetail : "解析任务启动后，这里会显示当前素材详细进度"
                            color: Theme.muted
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }
                    }
                }

                ThemedProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: hasCurrentItem ? currentProgress : 0
                }

                Text {
                    Layout.fillWidth: true
                    text: hasCurrentItem ? currentProgress + "% · 当前素材" : "0% · 当前素材"
                    color: Theme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
        }

        ScrollView {
            id: detailScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ScrollBar.vertical: ThemedScrollBar {
                policy: ScrollBar.AsNeeded
            }

            ColumnLayout {
                width: detailScroll.availableWidth
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    radius: 14
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    implicitHeight: batchCardColumn.implicitHeight + 22

                    ColumnLayout {
                        id: batchCardColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 11
                        spacing: 9

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: "视频解析总量"
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.Black
                                elide: Text.ElideRight
                            }

                            Text {
                                text: batchProgress + "%"
                                color: Theme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }

                        ThemedProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: batchProgress
                        }

                        Text {
                            Layout.fillWidth: true
                            text: viewModel ? viewModel.batchStatusText : "暂无视频解析批次。"
                            color: Theme.muted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10

                    StatTile {
                        label: "已处理"
                        value: viewModel ? viewModel.batchFinishedCount + "/" + viewModel.batchTotalCount : "0/0"
                    }

                    StatTile {
                        label: "成功"
                        value: metricValue(viewModel ? viewModel.batchSuccessfulCount : 0)
                    }

                    StatTile {
                        label: "失败"
                        value: metricValue(viewModel ? viewModel.batchFailedCount : 0)
                        valueColor: Theme.orange
                    }

                    StatTile {
                        label: "排队中"
                        value: metricValue(viewModel ? viewModel.batchQueuedCount : 0)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 14
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    implicitHeight: statusColumn.implicitHeight + 22

                    ColumnLayout {
                        id: statusColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 11
                        spacing: 7

                        Text {
                            Layout.fillWidth: true
                            text: "当前状态反馈"
                            color: Theme.text
                            font.pixelSize: 14
                            font.weight: Font.Black
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: hasCurrentItem ? viewModel.batchCurrentDetail : "当前没有正在处理的素材。"
                            color: Theme.text
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }
}

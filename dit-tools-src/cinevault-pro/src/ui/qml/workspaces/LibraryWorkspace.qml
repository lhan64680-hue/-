import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var viewModel

    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 6
                Text {
                    Layout.fillWidth: true
                    text: "素材库"
                    color: Theme.text
                    font.pixelSize: 28
                    font.weight: Font.Black
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: viewModel.statusText
                    color: Theme.muted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            Item { Layout.fillWidth: true }

            ActionButton {
                Layout.preferredWidth: 88
                Layout.preferredHeight: 36
                text: "大图卡片"
                primary: viewModel.viewMode === 0
                onClicked: viewModel.viewMode = 0
            }

            ActionButton {
                Layout.preferredWidth: 88
                Layout.preferredHeight: 36
                text: "技术表格"
                primary: viewModel.viewMode === 1
                onClicked: viewModel.viewMode = 1
            }
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: viewModel.viewMode === 0 ? gridComponent : tableComponent
        }
    }

    Component {
        id: gridComponent

        GridView {
            clip: true
            cellWidth: 240
            cellHeight: 282
            model: viewModel.model

            delegate: Item {
                width: 224
                height: 270

                AssetCard {
                    anchors.fill: parent
                    title: model.name
                    subtitle: model.relativePath
                    meta: model.sizeLabel + " · " + model.modifiedAt
                    tag: model.typeLabel
                    selected: viewModel.selectedAssetId === assetId
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: viewModel.selectAsset(assetId)
                }
            }
        }
    }

    Component {
        id: tableComponent

        Flickable {
            id: tableFlick
            clip: true
            contentWidth: Math.max(width, 980)
            contentHeight: height
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

            ListView {
                id: tableList
                width: tableFlick.contentWidth
                height: tableFlick.height
                clip: true
                spacing: 10
                model: viewModel.model

                header: Rectangle {
                    width: tableList.width
                    height: 46
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    radius: 16

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12
                        Text { text: "文件名"; color: Theme.muted; Layout.preferredWidth: 260; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: "类型"; color: Theme.muted; Layout.preferredWidth: 96; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: "大小"; color: Theme.muted; Layout.preferredWidth: 104; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: "修改时间"; color: Theme.muted; Layout.preferredWidth: 190; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: "相对路径"; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                    }
                }

                delegate: Rectangle {
                    width: tableList.width
                    height: 58
                    radius: 14
                    color: viewModel.selectedAssetId === assetId ? Qt.rgba(0.31, 0.55, 1.0, 0.14) : Theme.panel2
                    border.width: 1
                    border.color: viewModel.selectedAssetId === assetId ? Theme.blue : Theme.line

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12
                        Text { text: model.name; color: Theme.text; Layout.preferredWidth: 260; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: model.typeLabel; color: Theme.muted; Layout.preferredWidth: 96; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: model.sizeLabel; color: Theme.muted; Layout.preferredWidth: 104; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: model.modifiedAt; color: Theme.muted; Layout.preferredWidth: 190; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: model.relativePath; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideMiddle; verticalAlignment: Text.AlignVCenter }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: viewModel.selectAsset(assetId)
                    }
                }
            }
        }
    }
}

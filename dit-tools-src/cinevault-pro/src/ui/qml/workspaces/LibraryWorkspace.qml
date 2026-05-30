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
                    text: "素材库"
                    color: Theme.text
                    font.pixelSize: 28
                    font.weight: Font.Black
                }
                Text {
                    text: viewModel.statusText
                    color: Theme.muted
                    font.pixelSize: 13
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "大图卡片"
                onClicked: viewModel.viewMode = 0
            }

            Button {
                text: "技术表格"
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

        ListView {
            clip: true
            spacing: 10
            model: viewModel.model

            header: Rectangle {
                width: ListView.view.width
                height: 46
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line
                radius: 16

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    Text { text: "文件名"; color: Theme.muted; Layout.preferredWidth: 280 }
                    Text { text: "类型"; color: Theme.muted; Layout.preferredWidth: 110 }
                    Text { text: "大小"; color: Theme.muted; Layout.preferredWidth: 120 }
                    Text { text: "修改时间"; color: Theme.muted; Layout.preferredWidth: 220 }
                    Text { text: "相对路径"; color: Theme.muted; Layout.fillWidth: true }
                }
            }

            delegate: Rectangle {
                width: ListView.view.width
                height: 58
                radius: 14
                color: viewModel.selectedAssetId === assetId ? Qt.rgba(0.31, 0.55, 1.0, 0.14) : Theme.panel2
                border.width: 1
                border.color: viewModel.selectedAssetId === assetId ? Theme.blue : Theme.line

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    Text { text: model.name; color: Theme.text; Layout.preferredWidth: 280; elide: Text.ElideRight }
                    Text { text: model.typeLabel; color: Theme.muted; Layout.preferredWidth: 110 }
                    Text { text: model.sizeLabel; color: Theme.muted; Layout.preferredWidth: 120 }
                    Text { text: model.modifiedAt; color: Theme.muted; Layout.preferredWidth: 220; elide: Text.ElideRight }
                    Text { text: model.relativePath; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideRight }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: viewModel.selectAsset(assetId)
                }
            }
        }
    }
}

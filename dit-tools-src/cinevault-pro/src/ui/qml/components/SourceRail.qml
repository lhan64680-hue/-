import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var viewModel

    color: Theme.panel
    border.width: 1
    border.color: Theme.line

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: Theme.panel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                Text {
                    Layout.fillWidth: true
                    text: "素材源栏"
                    color: Theme.muted
                    font.pixelSize: 15
                    elide: Text.ElideRight
                }
                Item { Layout.fillWidth: true }
                Button {
                    Layout.preferredWidth: 62
                    Layout.preferredHeight: 34
                    text: "清除"
                    flat: true
                    onClicked: viewModel.clearSelection()
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: viewModel.model
            delegate: Item {
                width: ListView.view.width
                height: 124

                SourceCard {
                    anchors.fill: parent
                    anchors.margins: 14
                    active: viewModel.selectedSourceId === sourceId
                    name: model.name
                    metaText: totalFiles + " 个文件 · " + totalSize + "\n" + videoCount + " 个视频 · " + warningCount + " 个警告"
                    statusLabel: model.statusLabel
                    statusColor: model.statusColor
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: viewModel.selectSource(sourceId)
                }
            }
        }
    }
}

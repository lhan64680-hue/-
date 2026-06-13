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

        Text {
            Layout.fillWidth: true
            text: "检查/质检"
            color: Theme.text
            font.pixelSize: 28
            font.weight: Font.Black
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: "首版阶段先接入素材选中联动，后续在媒体处理阶段补视频流、音频流、时间码与异常重试。"
            color: Theme.muted
            wrapMode: Text.Wrap
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: viewModel.model

            delegate: Rectangle {
                width: ListView.view.width
                height: 72
                radius: 14
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    Text { text: model.name; color: Theme.text; Layout.preferredWidth: 220; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                    Text { text: model.typeLabel; color: Theme.muted; Layout.preferredWidth: 82; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                    Text { text: model.relativePath; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideMiddle; verticalAlignment: Text.AlignVCenter }
                    ActionButton {
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 34
                        text: "选中"
                        onClicked: viewModel.selectAsset(assetId)
                    }
                }
            }
        }
    }
}

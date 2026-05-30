import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Text {
            text: "报表工作室"
            color: Theme.text
            font.pixelSize: 28
            font.weight: Font.Black
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 22
            color: Theme.panel2
            border.width: 1
            border.color: Theme.line

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12

                Text {
                    text: "阶段 1-3 仅保留报表工作室占位和信息架构。"
                    color: Theme.text
                    font.pixelSize: 18
                }

                Text {
                    text: "后续阶段会在此接入模板、实时预览、PDF/Markdown/CSV/JSON 导出。"
                    color: Theme.muted
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}

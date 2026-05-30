import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var shellVm
    property var viewModel

    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Text {
            text: "导入"
            color: Theme.text
            font.pixelSize: 28
            font.weight: Font.Black
        }

        Text {
            text: shellVm.projectPath.length > 0 ? shellVm.projectPath : "尚未打开项目"
            color: Theme.muted
            font.pixelSize: 13
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 24
            color: Theme.panel2
            border.width: 1
            border.color: Theme.line

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width * 0.6
                spacing: 16

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "拖入素材卡、硬盘或项目目录"
                    color: Theme.text
                    font.pixelSize: 28
                    font.weight: Font.Black
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: "导入后将自动识别目录结构，生成项目内索引，并为后续缩略图与元数据处理保留队列。"
                    color: Theme.muted
                    wrapMode: Text.Wrap
                    font.pixelSize: 14
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: viewModel.summaryText
                    color: Theme.text
                    wrapMode: Text.Wrap
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: "添加素材源"
                    onClicked: shellVm.addSourceDirectory()
                }
            }
        }
    }
}

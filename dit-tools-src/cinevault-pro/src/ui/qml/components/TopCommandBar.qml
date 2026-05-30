import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var shellVm

    color: "#10131A"
    border.width: 1
    border.color: Theme.line
    implicitHeight: 64

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 16

        Text {
            text: "影资管家"
            color: Theme.text
            font.pixelSize: 22
            font.weight: Font.Black
        }

        Rectangle {
            color: Theme.panel2
            radius: 12
            border.width: 1
            border.color: Theme.line
            Layout.preferredWidth: 240
            Layout.preferredHeight: 40

            Text {
                anchors.centerIn: parent
                text: shellVm.projectName
                color: Theme.muted
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: [
                { label: "导入", value: 0 },
                { label: "素材库", value: 1 },
                { label: "检查/质检", value: 2 },
                { label: "报表", value: 3 },
                { label: "任务", value: 4 }
            ]

            delegate: Button {
                text: modelData.label
                flat: true
                onClicked: shellVm.currentWorkspace = modelData.value
                background: Rectangle {
                    radius: 18
                    color: shellVm.currentWorkspace === modelData.value ? Qt.rgba(0.31, 0.55, 1.0, 0.22) : "transparent"
                    border.width: shellVm.currentWorkspace === modelData.value ? 1 : 0
                    border.color: Qt.rgba(0.31, 0.55, 1.0, 0.36)
                }
                contentItem: Text {
                    text: parent.text
                    color: shellVm.currentWorkspace === modelData.value ? Theme.text : Theme.muted
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Item { Layout.fillWidth: true }

        TextField {
            Layout.preferredWidth: 320
            placeholderText: "搜索素材..."
            text: shellVm.globalSearchText
            onTextChanged: shellVm.globalSearchText = text
            color: Theme.text
            placeholderTextColor: Theme.muted
            background: Rectangle {
                color: Theme.panel2
                radius: 14
                border.width: 1
                border.color: Theme.line
            }
        }

        Button {
            text: "新建项目"
            onClicked: shellVm.createProject()
        }

        Button {
            text: "打开项目"
            onClicked: shellVm.openProject()
        }

        Button {
            text: "添加素材源"
            onClicked: shellVm.addSourceDirectory()
        }
    }
}

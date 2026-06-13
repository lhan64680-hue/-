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
        spacing: 8

        Text {
            text: "影资管家"
            color: Theme.text
            font.pixelSize: 22
            font.weight: Font.Black
            Layout.preferredWidth: 92
            elide: Text.ElideRight
        }

        Rectangle {
            color: Theme.panel2
            radius: 12
            border.width: 1
            border.color: Theme.line
            Layout.preferredWidth: 170
            Layout.minimumWidth: 130
            Layout.preferredHeight: 40

            Text {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                text: shellVm.projectName
                color: Theme.muted
                font.pixelSize: 13
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Repeater {
            model: [
                { label: "导入", value: 0, buttonWidth: 56 },
                { label: "素材库", value: 1, buttonWidth: 70 },
                { label: "检查/质检", value: 2, buttonWidth: 92 },
                { label: "报表", value: 3, buttonWidth: 56 },
                { label: "任务", value: 4, buttonWidth: 56 }
            ]

            delegate: Button {
                Layout.preferredWidth: modelData.buttonWidth
                Layout.preferredHeight: 36
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
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Item { Layout.fillWidth: true }

        TextField {
            Layout.preferredWidth: 220
            Layout.minimumWidth: 160
            Layout.maximumWidth: 260
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
            Layout.preferredWidth: 82
            Layout.preferredHeight: 36
            text: "新建项目"
            onClicked: shellVm.createProject()
            contentItem: Text {
                text: parent.text
                color: Theme.text
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Button {
            Layout.preferredWidth: 82
            Layout.preferredHeight: 36
            text: "打开项目"
            onClicked: shellVm.openProject()
            contentItem: Text {
                text: parent.text
                color: Theme.text
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Button {
            Layout.preferredWidth: 98
            Layout.preferredHeight: 36
            text: "添加素材源"
            onClicked: shellVm.addSourceDirectory()
            contentItem: Text {
                text: parent.text
                color: Theme.text
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}

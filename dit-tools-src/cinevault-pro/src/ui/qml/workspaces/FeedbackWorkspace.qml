import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import CineVault

Rectangle {
    id: root

    property var viewModel

    color: Theme.bg

    function submitDraftMessage(text) {
        if (viewModel) {
            viewModel.sendMessage(text)
        }
    }

    Component.onCompleted: if (viewModel) {
        viewModel.activate()
        viewModel.setWorkspaceActive(visible)
    }
    onVisibleChanged: if (viewModel) viewModel.setWorkspaceActive(visible)

    FileDialog {
        id: attachmentDialog
        title: "选择要发送给开发者的附件"
        fileMode: FileDialog.OpenFiles
        onAccepted: if (viewModel) viewModel.addAttachmentUrls(selectedFiles)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            radius: 22
            color: Theme.panel2
            border.width: 1
            border.color: Theme.line
            implicitHeight: 132

            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: viewModel ? viewModel.title : "使用反馈"
                        color: Theme.text
                        font.pixelSize: 28
                        font.weight: Font.Black
                    }

                    Text {
                        Layout.fillWidth: true
                        text: viewModel ? viewModel.subtitle : "反馈模块不可用"
                        color: Theme.muted
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: viewModel ? viewModel.statusMessage : ""
                        color: Theme.weak
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 84
                    radius: 18
                    color: Theme.card
                    border.width: 1
                    border.color: Theme.line

                    Column {
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: viewModel ? viewModel.unreadCount : 0
                            color: Theme.blue
                            font.pixelSize: 28
                            font.weight: Font.Black
                        }

                        Text {
                            text: viewModel ? viewModel.connectionStatus : "未连接"
                            color: Theme.muted
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: true
            sourceComponent: !viewModel ? unavailableComponent : (viewModel.needsProfile ? onboardingComponent : chatWorkspaceComponent)
        }
    }

    Component {
        id: unavailableComponent

        Item {
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 420)
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: "当前构建未启用反馈模块"
                    color: Theme.text
                    font.pixelSize: 24
                    font.weight: Font.Black
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.fillWidth: true
                    text: "请切换到完整工作流构建包后再使用“使用反馈”页面。"
                    color: Theme.muted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    Component {
        id: onboardingComponent

        Item {
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 520)
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    radius: 24
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    implicitHeight: 380

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 14

                        Text {
                            text: "建立你的长期反馈会话"
                            color: Theme.text
                            font.pixelSize: 24
                            font.weight: Font.Black
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "填写昵称和联系方式后，后续每次打开软件都会自动回到同一条反馈会话。你可以持续补充文本、图片、日志和压缩包，开发者回复后会实时显示。"
                            color: Theme.muted
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                        }

                        ThemedTextField {
                            id: nicknameField
                            Layout.fillWidth: true
                            placeholderText: "昵称，例如：李明 / 摄影组A机"
                        }

                        ThemedTextField {
                            id: contactField
                            Layout.fillWidth: true
                            placeholderText: "联系方式，例如：微信 / 手机 / 邮箱"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: Theme.card
                            border.width: 1
                            border.color: Theme.line
                            implicitHeight: 108

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 8

                                Text {
                                    text: "会自动附带的信息"
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "软件版本、系统信息、当前项目名与项目路径会跟随反馈一起发送，方便开发者更快定位问题。"
                                    color: Theme.muted
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        ActionButton {
                            Layout.alignment: Qt.AlignRight
                            Layout.preferredWidth: 132
                            Layout.preferredHeight: 40
                            text: viewModel && viewModel.busy ? "创建中..." : "开始反馈"
                            enabled: viewModel && !viewModel.busy
                            primary: true
                            onClicked: if (viewModel) viewModel.submitProfile(nicknameField.text, contactField.text)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: chatWorkspaceComponent

        RowLayout {
            spacing: 14

            Connections {
                target: viewModel
                function onMessageSubmitted(success) {
                    if (success) {
                        draftMessageArea.text = ""
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 22
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    ListView {
                        id: messageListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 12
                        model: viewModel ? viewModel.messageModel : null

                        ScrollBar.vertical: ThemedScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        onCountChanged: Qt.callLater(function() {
                            if (count > 0) {
                                positionViewAtIndex(count - 1, ListView.End)
                            }
                        })

                        delegate: Item {
                            width: ListView.view.width
                            height: bubble.implicitHeight

                            Rectangle {
                                id: bubble
                                width: Math.min(parent.width * 0.78, 720)
                                anchors.left: outgoing ? undefined : parent.left
                                anchors.right: outgoing ? parent.right : undefined
                                radius: 20
                                color: outgoing ? Theme.primaryBg : Theme.card
                                border.width: 1
                                border.color: outgoing ? Qt.rgba(1, 1, 1, 0.12) : Theme.line
                                implicitHeight: contentColumn.implicitHeight + 24

                                Column {
                                    id: contentColumn
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 10

                                    RowLayout {
                                        width: parent.width
                                        spacing: 8

                                        Text {
                                            text: senderLabel
                                            color: outgoing ? Theme.primaryText : Theme.text
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: createdAtLabel
                                            color: outgoing ? Qt.rgba(1, 1, 1, 0.82) : Theme.weak
                                            font.pixelSize: 11
                                        }
                                    }

                                    Text {
                                        visible: hasText
                                        width: parent.width
                                        text: model.text
                                        color: outgoing ? Theme.primaryText : Theme.text
                                        font.pixelSize: 14
                                        wrapMode: Text.Wrap
                                    }

                                    Flow {
                                        visible: hasAttachments
                                        width: parent.width
                                        spacing: 10

                                        Repeater {
                                            model: attachments

                                            delegate: Rectangle {
                                                width: modelData.isImage ? 180 : 220
                                                height: modelData.isImage ? 176 : 74
                                                radius: 16
                                                color: outgoing ? Qt.rgba(1, 1, 1, 0.12) : Theme.panel
                                                border.width: 1
                                                border.color: outgoing ? Qt.rgba(1, 1, 1, 0.18) : Theme.line
                                                clip: true

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: if (viewModel) viewModel.openAttachment(modelData.url)
                                                }

                                                Column {
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    spacing: 8

                                                    Image {
                                                        visible: modelData.isImage
                                                        width: parent.width
                                                        height: 96
                                                        fillMode: Image.PreserveAspectCrop
                                                        asynchronous: true
                                                        source: modelData.url
                                                    }

                                                    Text {
                                                        width: parent.width
                                                        text: modelData.name
                                                        color: outgoing ? Theme.primaryText : Theme.text
                                                        font.pixelSize: 12
                                                        font.weight: Font.DemiBold
                                                        elide: Text.ElideRight
                                                    }

                                                    Text {
                                                        width: parent.width
                                                        text: modelData.sizeLabel
                                                        color: outgoing ? Qt.rgba(1, 1, 1, 0.78) : Theme.weak
                                                        font.pixelSize: 11
                                                        elide: Text.ElideRight
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            visible: !messageListView.count
                            width: Math.min(parent.width - 24, 340)
                            height: 120
                            radius: 18
                            color: Theme.card
                            border.width: 1
                            border.color: Theme.line

                            Column {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "还没有反馈记录"
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "发第一条消息，把问题、截图或日志发给开发者。"
                                    color: Theme.muted
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 18
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.line
                        implicitHeight: composerColumn.implicitHeight + 24

                        ColumnLayout {
                            id: composerColumn
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            TextArea {
                                id: draftMessageArea
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                placeholderText: "描述你遇到的问题、复现步骤、期望结果，或直接附上截图和日志..."
                                wrapMode: TextEdit.Wrap
                                color: Theme.text
                                selectionColor: Theme.blue
                                selectedTextColor: Theme.primaryText
                                Keys.onPressed: function(event) {
                                    if (draftMessageArea.inputMethodComposing) {
                                        return
                                    }
                                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                            && !(event.modifiers & Qt.ControlModifier)
                                            && !(event.modifiers & Qt.ShiftModifier)
                                            && !(event.modifiers & Qt.AltModifier)
                                            && !(event.modifiers & Qt.MetaModifier)) {
                                        event.accepted = true
                                        root.submitDraftMessage(draftMessageArea.text)
                                    }
                                }
                                background: Rectangle {
                                    radius: 14
                                    color: Theme.panel
                                    border.width: 1
                                    border.color: draftMessageArea.activeFocus ? Theme.blue : Theme.line
                                }
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                visible: viewModel && viewModel.pendingAttachments.length > 0

                                Repeater {
                                    model: viewModel ? viewModel.pendingAttachments : []

                                    delegate: Rectangle {
                                        radius: 999
                                        color: Theme.selectedBg
                                        border.width: 1
                                        border.color: Theme.selectedLine
                                        implicitHeight: 32
                                        implicitWidth: label.implicitWidth + 38

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 8

                                            Text {
                                                id: label
                                                text: modelData.name + " · " + modelData.sizeLabel
                                                color: Theme.text
                                                font.pixelSize: 12
                                            }

                                            Text {
                                                text: "×"
                                                color: Theme.muted
                                                font.pixelSize: 14

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: if (viewModel) viewModel.removePendingAttachment(index)
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                ActionButton {
                                    Layout.preferredWidth: 110
                                    Layout.preferredHeight: 38
                                    text: "添加附件"
                                    onClicked: attachmentDialog.open()
                                }

                                ActionButton {
                                    Layout.preferredWidth: 92
                                    Layout.preferredHeight: 38
                                    text: "刷新"
                                    onClicked: if (viewModel) viewModel.refresh()
                                }

                                Item { Layout.fillWidth: true }

                                ActionButton {
                                    Layout.preferredWidth: 128
                                    Layout.preferredHeight: 38
                                    text: viewModel && viewModel.sending ? "发送中..." : "发送反馈"
                                    enabled: viewModel && !viewModel.sending
                                    primary: true
                                    onClicked: root.submitDraftMessage(draftMessageArea.text)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                radius: 22
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 16

                    Text {
                        text: "会话详情"
                        color: Theme.text
                        font.pixelSize: 20
                        font.weight: Font.Black
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 18
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.line
                        implicitHeight: 110

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Text {
                                text: "资料"
                                color: Theme.muted
                                font.pixelSize: 12
                            }

                            Text {
                                Layout.fillWidth: true
                                text: viewModel ? viewModel.profileSummary : ""
                                color: Theme.text
                                font.pixelSize: 14
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 18
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.line
                        implicitHeight: 122

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Text {
                                text: "项目上下文"
                                color: Theme.muted
                                font.pixelSize: 12
                            }

                            Text {
                                Layout.fillWidth: true
                                text: viewModel ? viewModel.projectSummary : ""
                                color: Theme.text
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 10

                        Repeater {
                            model: [
                                { title: "处理状态", value: viewModel ? viewModel.conversationStatusLabel : "" },
                                { title: "软件版本", value: viewModel ? viewModel.appVersionLabel : "" },
                                { title: "最近更新", value: viewModel ? viewModel.latestUpdatedAt : "" },
                                { title: "实时状态", value: viewModel ? viewModel.connectionStatus : "" }
                            ]

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 92
                                radius: 18
                                color: Theme.card
                                border.width: 1
                                border.color: Theme.line

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Text {
                                        text: modelData.title
                                        color: Theme.muted
                                        font.pixelSize: 12
                                    }

                                    Text {
                                        width: parent.width
                                        text: modelData.value
                                        color: Theme.text
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 18
                        color: Theme.selectedBg
                        border.width: 1
                        border.color: Theme.selectedLine
                        implicitHeight: 86

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Text {
                                text: "提醒"
                                color: Theme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            Text {
                                width: parent.width
                                text: "若开发者在你离线时回复，本页在下次打开软件或重新联网后会自动补收历史消息。"
                                color: Theme.muted
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
        }
    }
}

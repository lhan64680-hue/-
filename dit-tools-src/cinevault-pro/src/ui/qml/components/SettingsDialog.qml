import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Dialog {
    id: root

    property var viewModel
    property string draftVisionBaseUrl: ""
    property string draftVisionApiKey: ""
    property string draftVisionModel: ""
    property int draftAnalysisMode: 0
    property int draftFrameInterval: 10
    property int draftThumbnailFrameIndex: 3
    property int draftContactSheetFrameCount: 24
    property int draftAnalysisTimeoutSec: 60
    property int draftThemeMode: Theme.modeSystem
    property int bodyFontSize: 15
    property int sectionTitleSize: 20
    property int controlHeight: 42
    property int sectionPadding: 20
    property int formLabelWidth: 116

    modal: true
    width: 900
    height: 700
    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    onOpened: {
        if (viewModel) {
            viewModel.refresh()
            draftVisionBaseUrl = viewModel.visionBaseUrl
            draftVisionApiKey = viewModel.visionApiKey
            draftVisionModel = viewModel.visionModel
            draftAnalysisMode = viewModel.analysisMode
            draftFrameInterval = viewModel.frameInterval
            draftThumbnailFrameIndex = viewModel.thumbnailFrameIndex
            draftContactSheetFrameCount = viewModel.contactSheetFrameCount
            draftAnalysisTimeoutSec = viewModel.analysisTimeoutSec
            draftThemeMode = viewModel.themeMode
        }
    }

    background: Rectangle {
        radius: 22
        color: Theme.bg
        border.width: 1
        border.color: Theme.line
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 78
            color: Theme.panel2
            border.width: 1
            border.color: Theme.line

            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                ColumnLayout {
                    spacing: 4

                    Text {
                        text: "设置"
                        color: Theme.text
                        font.pixelSize: 26
                        font.weight: Font.Black
                    }

                    Text {
                        text: "视觉解析、缩略图和解析图片配置"
                        color: Theme.muted
                        font.pixelSize: root.bodyFontSize
                    }
                }

                Item { Layout.fillWidth: true }

                ActionButton {
                    Layout.preferredWidth: 118
                    Layout.preferredHeight: root.controlHeight
                    text: "保存并应用"
                    primary: true
                    textPixelSize: root.bodyFontSize
                    onClicked: if (viewModel) {
                        viewModel.saveAndApply(
                            root.draftVisionBaseUrl,
                            root.draftVisionApiKey,
                            root.draftVisionModel,
                            root.draftAnalysisMode,
                            root.draftFrameInterval,
                            root.draftThumbnailFrameIndex,
                            root.draftContactSheetFrameCount,
                            root.draftAnalysisTimeoutSec)
                    }
                }

                ActionButton {
                    Layout.preferredWidth: 78
                    Layout.preferredHeight: root.controlHeight
                    text: "关闭"
                    textPixelSize: root.bodyFontSize
                    onClicked: root.close()
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: root.width
                spacing: 18
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 20

                Rectangle {
                    Layout.fillWidth: true
                    radius: 18
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    implicitHeight: appearanceContent.implicitHeight + root.sectionPadding * 2

                    ColumnLayout {
                        id: appearanceContent
                        anchors.fill: parent
                        anchors.margins: root.sectionPadding
                        spacing: 12

                        Text {
                            text: "外观"
                            color: Theme.text
                            font.pixelSize: root.sectionTitleSize
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                Layout.preferredWidth: 88
                                Layout.alignment: Qt.AlignVCenter
                                text: "主题模式"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedComboBox {
                                Layout.preferredWidth: 180
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                model: [
                                    { label: "跟随系统", value: Theme.modeSystem },
                                    { label: "暗色", value: Theme.modeDark },
                                    { label: "浅色", value: Theme.modeLight }
                                ]
                                textRole: "label"
                                currentIndex: root.draftThemeMode
                                onActivated: {
                                    root.draftThemeMode = model[index].value
                                    if (viewModel) {
                                        viewModel.themeMode = root.draftThemeMode
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 18
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    implicitHeight: visionContent.implicitHeight + root.sectionPadding * 2

                    ColumnLayout {
                        id: visionContent
                        anchors.fill: parent
                        anchors.margins: root.sectionPadding
                        spacing: 14

                        Text {
                            text: "视觉解析"
                            color: Theme.text
                            font.pixelSize: root.sectionTitleSize
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 14
                            Layout.fillWidth: true

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "Base URL"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedTextField {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                text: root.draftVisionBaseUrl
                                placeholderText: "https://api.openai.com/v1"
                                onTextEdited: root.draftVisionBaseUrl = text
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "API Key"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedTextField {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                text: root.draftVisionApiKey
                                echoMode: TextInput.Password
                                placeholderText: "输入视觉接口密钥"
                                onTextEdited: root.draftVisionApiKey = text
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "模型名"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedTextField {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                text: root.draftVisionModel
                                placeholderText: "gpt-4.1-mini"
                                onTextEdited: root.draftVisionModel = text
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "抽帧模式"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedComboBox {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                model: [
                                    { label: "每10帧抽1帧", value: 0 },
                                    { label: "逐帧解析", value: 1 }
                                ]
                                textRole: "label"
                                currentIndex: root.draftAnalysisMode === 1 ? 1 : 0
                                onActivated: root.draftAnalysisMode = model[index].value
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "抽帧间隔"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSpinBox {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                from: 1
                                to: 240
                                value: root.draftFrameInterval
                                enabled: root.draftAnalysisMode === 0
                                onValueModified: root.draftFrameInterval = value
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "超时（秒）"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSpinBox {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                from: 5
                                to: 600
                                value: root.draftAnalysisTimeoutSec
                                onValueModified: root.draftAnalysisTimeoutSec = value
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ActionButton {
                                Layout.preferredWidth: 150
                                Layout.preferredHeight: root.controlHeight
                                text: "测试连通状态"
                                primary: true
                                textPixelSize: root.bodyFontSize
                                onClicked: if (viewModel) {
                                    viewModel.testConnectionWith(
                                        root.draftVisionBaseUrl,
                                        root.draftVisionApiKey,
                                        root.draftVisionModel,
                                        root.draftAnalysisTimeoutSec)
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: viewModel ? viewModel.lastMessage : ""
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 18
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    implicitHeight: thumbnailContent.implicitHeight + root.sectionPadding * 2

                    ColumnLayout {
                        id: thumbnailContent
                        anchors.fill: parent
                        anchors.margins: root.sectionPadding
                        spacing: 12

                        Text {
                            text: "缩略图"
                            color: Theme.text
                            font.pixelSize: root.sectionTitleSize
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                text: "默认取第几帧作为缩略图"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSpinBox {
                                Layout.preferredWidth: 150
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                from: 1
                                to: 120
                                value: root.draftThumbnailFrameIndex
                                onValueModified: root.draftThumbnailFrameIndex = value
                            }

                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                text: "右侧详情多宫格数量"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSpinBox {
                                Layout.preferredWidth: 150
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                from: 1
                                to: 64
                                value: root.draftContactSheetFrameCount
                                onValueModified: root.draftContactSheetFrameCount = value
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 18
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line
                    implicitHeight: frameCacheContent.implicitHeight + root.sectionPadding * 2

                    ColumnLayout {
                        id: frameCacheContent
                        anchors.fill: parent
                        anchors.margins: root.sectionPadding
                        spacing: 12

                        Text {
                            text: "解析图片"
                            color: Theme.text
                            font.pixelSize: root.sectionTitleSize
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "实际数据目录：" + (viewModel ? viewModel.dataRootPath : "")
                            color: Theme.muted
                            font.pixelSize: root.bodyFontSize
                            wrapMode: Text.WrapAnywhere
                        }

                        Text {
                            text: "解析图片占用：" + (viewModel ? viewModel.frameCacheSizeLabel : "0B")
                            color: Theme.muted
                            font.pixelSize: root.bodyFontSize
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ActionButton {
                                Layout.preferredWidth: 132
                                Layout.preferredHeight: root.controlHeight
                                text: "刷新图片信息"
                                textPixelSize: root.bodyFontSize
                                onClicked: if (viewModel) viewModel.refreshCacheInfo()
                            }
                        }
                    }
                }
            }
        }
    }
}

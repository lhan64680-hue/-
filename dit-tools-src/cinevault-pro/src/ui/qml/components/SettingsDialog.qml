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
    property bool draftSearchAssistantEnabled: true
    property bool draftFrameRerankEnabled: true
    property bool draftLocalOnlySearch: false
    property bool draftAllowSearchFrameUpload: true
    property bool draftQuickSearchEnabled: true
    property string draftQuickSearchShortcut: "Alt+Space"
    property bool draftStartAtLogin: false
    property int draftCloseButtonBehavior: 0
    property int draftAnalysisMode: 0
    property int draftFrameInterval: 10
    property int draftThumbnailFrameIndex: 3
    property int draftContactSheetFrameCount: 24
    property int draftAnalysisTimeoutSec: 60
    property int draftThemeMode: Theme.modeSystem
    property int draftUpdateDownloadMode: 0
    property string draftUpdateManualProxyUrl: ""
    property bool draftAutoInstallUpdates: false
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
            draftSearchAssistantEnabled = viewModel.searchAssistantEnabled
            draftFrameRerankEnabled = viewModel.frameRerankEnabled
            draftLocalOnlySearch = viewModel.localOnlySearch
            draftAllowSearchFrameUpload = viewModel.allowSearchFrameUpload
            draftQuickSearchEnabled = viewModel.quickSearchEnabled
            draftQuickSearchShortcut = viewModel.quickSearchShortcut
            draftStartAtLogin = viewModel.startAtLogin
            draftCloseButtonBehavior = viewModel.closeButtonBehavior
            draftAnalysisMode = viewModel.analysisMode
            draftFrameInterval = viewModel.frameInterval
            draftThumbnailFrameIndex = viewModel.thumbnailFrameIndex
            draftContactSheetFrameCount = viewModel.contactSheetFrameCount
            draftAnalysisTimeoutSec = viewModel.analysisTimeoutSec
            draftThemeMode = viewModel.themeMode
            draftUpdateDownloadMode = viewModel.updateDownloadMode
            draftUpdateManualProxyUrl = viewModel.updateManualProxyUrl
            draftAutoInstallUpdates = viewModel.autoInstallUpdates
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
            Layout.preferredHeight: 96
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
                        text: viewModel ? viewModel.currentVersionLabel : "当前版本：v0.0.0"
                        color: Theme.weak
                        font.pixelSize: 13
                    }

                    Text {
                        text: "软件更新、视觉解析、缩略图和解析图片配置"
                        color: Theme.muted
                        font.pixelSize: root.bodyFontSize
                    }
                }

                Item { Layout.fillWidth: true }

                ActionButton {
                    Layout.preferredWidth: 118
                    Layout.preferredHeight: root.controlHeight
                    text: viewModel && viewModel.updateBusy ? "检查中..." : "检查更新"
                    enabled: viewModel && !viewModel.updateBusy
                    textPixelSize: root.bodyFontSize
                    onClicked: if (viewModel) {
                        viewModel.saveUpdateDownloadSettings(
                            root.draftUpdateDownloadMode,
                            root.draftUpdateManualProxyUrl)
                        viewModel.checkForUpdates()
                    }
                }

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
                            root.draftSearchAssistantEnabled,
                            root.draftFrameRerankEnabled,
                            root.draftLocalOnlySearch,
                            root.draftAllowSearchFrameUpload,
                            root.draftQuickSearchEnabled,
                            root.draftQuickSearchShortcut,
                            root.draftStartAtLogin,
                            root.draftCloseButtonBehavior,
                            root.draftAnalysisMode,
                            root.draftFrameInterval,
                            root.draftThumbnailFrameIndex,
                            root.draftContactSheetFrameCount,
                            root.draftAnalysisTimeoutSec,
                            root.draftUpdateDownloadMode,
                            root.draftUpdateManualProxyUrl)
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
                    implicitHeight: updateContent.implicitHeight + root.sectionPadding * 2

                    ColumnLayout {
                        id: updateContent
                        anchors.fill: parent
                        anchors.margins: root.sectionPadding
                        spacing: 14

                        Text {
                            text: "软件更新"
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
                                text: "自动更新"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSwitch {
                                checked: root.draftAutoInstallUpdates
                                text: checked ? "下载完成后自动安装" : "下载完成后询问"
                                font.pixelSize: root.bodyFontSize
                                onToggled: {
                                    root.draftAutoInstallUpdates = checked
                                    if (viewModel) {
                                        viewModel.autoInstallUpdates = checked
                                    }
                                }
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "下载更新渠道"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedComboBox {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                model: [
                                    { label: "自动检测代理", value: 0 },
                                    { label: "手动代理", value: 1 },
                                    { label: "直连下载", value: 2 }
                                ]
                                textRole: "label"
                                currentIndex: root.draftUpdateDownloadMode === 1 ? 1 : (root.draftUpdateDownloadMode === 2 ? 2 : 0)
                                onActivated: root.draftUpdateDownloadMode = model[index].value
                            }

                            Text {
                                visible: root.draftUpdateDownloadMode === 1
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "代理地址"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedTextField {
                                visible: root.draftUpdateDownloadMode === 1
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                text: root.draftUpdateManualProxyUrl
                                placeholderText: "http://127.0.0.1:7890"
                                onTextEdited: root.draftUpdateManualProxyUrl = text
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
                    implicitHeight: quickSearchContent.implicitHeight + root.sectionPadding * 2

                    ColumnLayout {
                        id: quickSearchContent
                        anchors.fill: parent
                        anchors.margins: root.sectionPadding
                        spacing: 12

                        Text {
                            text: "快捷搜索"
                            color: Theme.text
                            font.pixelSize: root.sectionTitleSize
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "像 Flow Launcher 一样，从任何窗口拉起独立搜索框，直接使用自然语言搜索全部项目的文件夹、画面和素材。"
                            color: Theme.muted
                            font.pixelSize: root.bodyFontSize
                            wrapMode: Text.Wrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 12

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "全局唤起"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSwitch {
                                checked: root.draftQuickSearchEnabled
                                text: checked ? "已启用" : "已关闭"
                                font.pixelSize: root.bodyFontSize
                                onToggled: root.draftQuickSearchEnabled = checked
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "快捷键"
                                color: root.draftQuickSearchEnabled ? Theme.muted : Theme.weak
                                font.pixelSize: root.bodyFontSize
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                enabled: root.draftQuickSearchEnabled
                                spacing: 10

                                ThemedTextField {
                                    id: quickSearchShortcutRecorder
                                    Layout.preferredWidth: 220
                                    Layout.preferredHeight: root.controlHeight
                                    readOnly: true
                                    selectByMouse: true
                                    text: root.draftQuickSearchShortcut
                                    placeholderText: "点击后按下组合键"
                                    Keys.onPressed: function(event) {
                                        if (!viewModel) {
                                            return
                                        }
                                        if (event.key === Qt.Key_Escape) {
                                            quickSearchShortcutRecorder.focus = false
                                            event.accepted = true
                                            return
                                        }
                                        var shortcut = viewModel.shortcutFromKeyEvent(event.key, event.modifiers)
                                        if (shortcut.length > 0) {
                                            root.draftQuickSearchShortcut = shortcut
                                        }
                                        event.accepted = true
                                    }
                                }

                                ActionButton {
                                    Layout.preferredWidth: 104
                                    Layout.preferredHeight: root.controlHeight
                                    text: "恢复默认"
                                    onClicked: root.draftQuickSearchShortcut = "Alt+Space"
                                }

                                Item { Layout.fillWidth: true }
                            }

                            Text {
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "开机启动"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSwitch {
                                checked: root.draftStartAtLogin
                                text: checked ? "登录后在托盘运行" : "不自动启动"
                                font.pixelSize: root.bodyFontSize
                                onToggled: root.draftStartAtLogin = checked
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: viewModel ? viewModel.quickSearchStatusText : ""
                            color: text.indexOf("失败") >= 0 || text.indexOf("占用") >= 0 ? Theme.red : Theme.blue
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "保存后立即重新注册快捷键。关闭主窗口时程序保留在托盘；可从托盘菜单重新显示或彻底退出。"
                            color: Theme.weak
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                Layout.preferredWidth: 88
                                Layout.alignment: Qt.AlignVCenter
                                text: "关闭按钮"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedComboBox {
                                Layout.preferredWidth: 220
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                model: [
                                    { label: "每次询问", value: 0 },
                                    { label: "最小化到托盘", value: 1 },
                                    { label: "直接退出软件", value: 2 }
                                ]
                                textRole: "label"
                                currentIndex: root.draftCloseButtonBehavior
                                onActivated: root.draftCloseButtonBehavior = model[index].value
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "选择点击主窗口关闭按钮时的默认行为"
                                color: Theme.weak
                                font.pixelSize: 12
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
                                    { label: "逐帧解析", value: 1 },
                                    { label: "自定义间隔", value: 2 }
                                ]
                                textRole: "label"
                                currentIndex: root.draftAnalysisMode === 1 ? 1 : (root.draftAnalysisMode === 2 ? 2 : 0)
                                onActivated: root.draftAnalysisMode = model[index].value
                            }

                            Text {
                                visible: root.draftAnalysisMode === 2
                                Layout.preferredWidth: root.formLabelWidth
                                Layout.alignment: Qt.AlignVCenter
                                text: "抽帧间隔"
                                color: Theme.muted
                                font.pixelSize: root.bodyFontSize
                            }

                            ThemedSpinBox {
                                visible: root.draftAnalysisMode === 2
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.controlHeight
                                font.pixelSize: root.bodyFontSize
                                from: 1
                                to: 240
                                value: root.draftFrameInterval
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

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 14
                            color: Qt.rgba(0.22, 0.48, 0.84, 0.08)
                            border.width: 1
                            border.color: Qt.rgba(0.22, 0.48, 0.84, 0.28)
                            implicitHeight: searchAssistSettings.implicitHeight + 28

                            ColumnLayout {
                                id: searchAssistSettings
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8

                                Text {
                                    text: "模型辅助搜索与隐私"
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }

                                ThemedSwitch {
                                    checked: root.draftLocalOnlySearch
                                    text: "仅本地搜索（不发起任何搜索模型网络请求）"
                                    font.pixelSize: root.bodyFontSize
                                    onToggled: root.draftLocalOnlySearch = checked
                                }

                                ThemedSwitch {
                                    enabled: !root.draftLocalOnlySearch
                                    checked: root.draftSearchAssistantEnabled
                                    text: "使用视觉语言模型辅助理解自然语言查询"
                                    font.pixelSize: root.bodyFontSize
                                    onToggled: root.draftSearchAssistantEnabled = checked
                                }

                                ThemedSwitch {
                                    enabled: !root.draftLocalOnlySearch
                                    checked: root.draftFrameRerankEnabled
                                    text: "使用视觉语言模型复核前 8 个候选帧"
                                    font.pixelSize: root.bodyFontSize
                                    onToggled: root.draftFrameRerankEnabled = checked
                                }

                                ThemedSwitch {
                                    enabled: !root.draftLocalOnlySearch && root.draftFrameRerankEnabled
                                    checked: root.draftAllowSearchFrameUpload
                                    text: "允许将候选帧缩略图发送到已配置的模型接口"
                                    font.pixelSize: root.bodyFontSize
                                    onToggled: root.draftAllowSearchFrameUpload = checked
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "查询理解只发送搜索文字；候选帧复核会发送最多 8 张缩略图和对应候选 ID。关闭缩略图授权后仍保留本地帧搜索与中央帧卡片。"
                                    color: Theme.muted
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    id: root

    property var viewModel
    property var shellVm
    property string imageViewerSource: ""
    property real imageViewerScale: 1.0
    property real imageViewerOffsetX: 0
    property real imageViewerOffsetY: 0

    function imageFileSource(path) {
        return path && path.length > 0 && localImageUrlHelper
            ? localImageUrlHelper.sourceForInput(path)
            : ""
    }

    function openImageViewer(source) {
        var normalizedSource = String(source || "")
        if (normalizedSource.length === 0) {
            return
        }
        imageViewerSource = normalizedSource
        imageViewerScale = 1.0
        imageViewerOffsetX = 0
        imageViewerOffsetY = 0
        imageViewerOverlay.forceActiveFocus()
    }

    function closeImageViewer() {
        imageViewerSource = ""
    }

    color: Theme.bg
    focus: true

    Keys.onPressed: function(event) {
        if (!viewModel || root.imageViewerSource.length > 0) {
            return
        }
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
            viewModel.moveVideoSelection(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
            viewModel.moveVideoSelection(1)
            event.accepted = true
        }
    }

    Component.onCompleted: if (viewModel) viewModel.reload()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            radius: 20
            color: Theme.panel2
            border.width: 1
            border.color: Theme.line
            implicitHeight: 138

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 6

                        Text {
                            text: "素材管理中心"
                            color: Theme.text
                            font.pixelSize: 28
                            font.weight: Font.Black
                        }

                        Text {
                            text: viewModel ? viewModel.statusText : ""
                            color: Theme.muted
                            font.pixelSize: 13
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ActionButton {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36
                        text: "同步当前项目"
                        onClicked: if (viewModel) viewModel.syncCurrentProject()
                    }

                    ActionButton {
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 36
                        text: "批量解析"
                        enabled: viewModel
                        onClicked: {
                            if (viewModel) {
                                if (viewModel.hasAnalyzedVisible) {
                                    batchAnalyzeDialog.open()
                                } else {
                                    viewModel.analyzeVisiblePending()
                                }
                            }
                        }
                    }

                    ActionButton {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36
                        text: "重建全局索引"
                        primary: true
                        onClicked: if (viewModel) viewModel.rebuildGlobalIndex()
                    }

                    ActionButton {
                        Layout.preferredWidth: 88
                        Layout.preferredHeight: 36
                        text: "全部确认"
                        enabled: viewModel && viewModel.canConfirmVisible
                        onClicked: if (viewModel) viewModel.confirmVisible()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ThemedTextField {
                        Layout.preferredWidth: 240
                        Layout.fillWidth: true
                        placeholderText: "搜索摘要、关键词、项目名或文件名"
                        text: shellVm ? shellVm.globalSearchText : ""
                        onTextChanged: if (shellVm) shellVm.globalSearchText = text
                    }

                    ThemedComboBox {
                        Layout.preferredWidth: 160
                        model: viewModel ? viewModel.projectOptions : []
                        textRole: "label"
                        onActivated: if (viewModel) viewModel.setProjectFilter(model[index].value)
                    }

                    ThemedComboBox {
                        Layout.preferredWidth: 160
                        model: viewModel ? viewModel.sourceOptions : []
                        textRole: "label"
                        onActivated: if (viewModel) viewModel.setSourceFilter(model[index].value)
                    }

                    ThemedComboBox {
                        Layout.preferredWidth: 120
                        model: viewModel ? viewModel.analysisStatusOptions : []
                        textRole: "label"
                        onActivated: if (viewModel) viewModel.setAnalysisStatusFilter(model[index].value)
                    }

                    ThemedComboBox {
                        Layout.preferredWidth: 140
                        model: viewModel ? viewModel.confirmationStatusOptions : []
                        textRole: "label"
                        onActivated: if (viewModel) viewModel.setConfirmationStatusFilter(model[index].value)
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: viewModel && viewModel.message.length > 0
            text: viewModel ? viewModel.message : ""
            color: Theme.muted
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 20
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line

                ListView {
                    id: resultList
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    spacing: 10
                    reuseItems: true
                    cacheBuffer: 720
                    model: viewModel ? viewModel.model : null
                    currentIndex: viewModel ? viewModel.selectedVideoIndex : -1

                    ScrollBar.vertical: ThemedScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    onCurrentIndexChanged: if (currentIndex >= 0) {
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: errorMessage.length > 0 ? 138 : 118
                        radius: 18
                        color: viewModel && viewModel.selectedVideoKey === videoKey ? Theme.selectedBg : Theme.bg
                        border.width: 1
                        border.color: viewModel && viewModel.selectedVideoKey === videoKey ? Theme.blue : Theme.line

                        MouseArea {
                            anchors.fill: parent
                            anchors.rightMargin: 112
                            onClicked: {
                                root.forceActiveFocus()
                                if (viewModel) {
                                    viewModel.selectVideo(videoKey)
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 164
                                Layout.preferredHeight: 92
                                radius: 14
                                color: Theme.mediaSurface
                                border.width: 1
                                border.color: Theme.line
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    source: root.imageFileSource(thumbnailPath)
                                    sourceSize.width: Math.max(1, Math.round(width))
                                    sourceSize.height: Math.max(1, Math.round(height))
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: thumbnailPath.length === 0
                                    text: "暂无缩略图"
                                    color: Theme.muted
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: thumbnailPath.length > 0
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.openImageViewer(root.imageFileSource(thumbnailPath))
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: fileName
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: projectName + " · " + sourceName
                                    color: Theme.muted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: summary.length > 0 ? summary : "尚未生成视频内容摘要"
                                    color: Theme.text
                                    font.pixelSize: 13
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: errorMessage.length > 0
                                    text: "失败原因：" + errorMessage
                                    color: Theme.orange
                                    font.pixelSize: 12
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Text {
                                        text: keywords.length > 0 ? keywords : "无关键词"
                                        color: Theme.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: analysisStatusLabel
                                        color: Theme.text
                                        font.pixelSize: 12
                                    }

                                    Text {
                                        text: confirmationStatusLabel
                                        color: Theme.muted
                                        font.pixelSize: 12
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.preferredWidth: 88
                                Layout.fillHeight: true
                                spacing: 6

                                Item { Layout.fillHeight: true }

                                ActionButton {
                                    Layout.preferredWidth: 82
                                    Layout.preferredHeight: 32
                                    text: isConfirmed ? "已确认" : "确认"
                                    primary: !isConfirmed
                                    enabled: viewModel && !isConfirmed
                                    onClicked: if (viewModel) viewModel.confirmVideo(videoKey)
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }

                    }

                    Text {
                        anchors.centerIn: parent
                        visible: resultList.count === 0
                        text: "当前筛选条件下没有素材。"
                        color: Theme.muted
                        font.pixelSize: 14
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 420
                Layout.fillHeight: true
                radius: 20
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line

                ScrollView {
                    id: detailScroll

                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true

                    ColumnLayout {
                        width: detailScroll.availableWidth
                        spacing: 12

                        Text {
                            Layout.fillWidth: true
                            text: viewModel && viewModel.hasSelection ? viewModel.selectedTitle : "选择左侧视频查看详情"
                            color: Theme.text
                            font.pixelSize: 20
                            font.weight: Font.Black
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 210
                            radius: 18
                            color: Theme.mediaSurface
                            border.width: 1
                            border.color: Theme.line
                            clip: true

                            Image {
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true
                                source: viewModel ? viewModel.selectedThumbnailUrl : ""
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !viewModel || !viewModel.hasSelection || viewModel.selectedThumbnailUrl.toString().length === 0
                                text: viewModel && viewModel.selectedFramesLoading ? "正在加载预览..." : "暂无多宫格拼图"
                                color: Theme.muted
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: viewModel && viewModel.hasSelection && viewModel.selectedThumbnailUrl.toString().length > 0
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.openImageViewer(viewModel.selectedThumbnailUrl.toString())
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: viewModel && viewModel.hasSelection
                            text: (viewModel ? viewModel.selectedProjectName : "") + " · " + (viewModel ? viewModel.selectedSourceName : "")
                            color: Theme.muted
                            font.pixelSize: 12
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ActionButton {
                                Layout.preferredWidth: 122
                                Layout.preferredHeight: 34
                                text: viewModel ? viewModel.analyzeButtonText : "开始解析"
                                primary: true
                                enabled: viewModel && viewModel.canAnalyzeSelected
                                onClicked: if (viewModel) viewModel.analyzeSelected()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ActionButton {
                                Layout.preferredWidth: 98
                                Layout.preferredHeight: 34
                                text: "打开所属项目"
                                enabled: viewModel && viewModel.hasSelection
                                onClicked: if (viewModel) viewModel.openSelectedProject()
                            }

                            ActionButton {
                                Layout.preferredWidth: 88
                                Layout.preferredHeight: 34
                                text: "定位文件夹"
                                enabled: viewModel && viewModel.hasSelection
                                onClicked: if (viewModel) viewModel.locateSelectedSource()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            visible: viewModel && viewModel.hasSelection
                            radius: 16
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.line
                            implicitHeight: analysisError.visible ? 118 : 92

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        Layout.fillWidth: true
                                        text: viewModel ? viewModel.selectedAnalysisProgressText : ""
                                        color: Theme.text
                                        font.pixelSize: 13
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: (viewModel ? viewModel.selectedAnalysisProgress : 0) + "%"
                                        color: Theme.muted
                                        font.pixelSize: 12
                                    }
                                }

                                ThemedProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: viewModel ? viewModel.selectedAnalysisProgress : 0
                                    indeterminate: viewModel && viewModel.selectedAnalysisBusy && viewModel.selectedAnalysisProgress <= 0
                                }

                                Text {
                                    id: analysisError
                                    Layout.fillWidth: true
                                    visible: viewModel && viewModel.selectedAnalysisError.length > 0
                                    text: "失败原因：" + (viewModel ? viewModel.selectedAnalysisError : "")
                                    color: Theme.orange
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Rectangle {
                            id: summaryCard

                            Layout.fillWidth: true
                            radius: 16
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.line
                            clip: true
                            implicitHeight: Math.max(140, summaryColumn.implicitHeight + 28)

                            ColumnLayout {
                                id: summaryColumn

                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 10

                                Text {
                                    text: "视频摘要"
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: viewModel && viewModel.selectedSummary.length > 0
                                        ? viewModel.selectedSummary
                                        : "当前还没有摘要。先执行解析任务，完成后这里会显示视频内容概述。"
                                    color: Theme.muted
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        Rectangle {
                            id: keywordCard

                            Layout.fillWidth: true
                            radius: 16
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.line
                            clip: true
                            implicitHeight: Math.max(150, keywordColumn.implicitHeight + 28)

                            ColumnLayout {
                                id: keywordColumn

                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 10

                                Text {
                                    text: "关键词与状态"
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }

                                Flow {
                                    id: keywordFlow

                                    Layout.fillWidth: true
                                    spacing: 8

                                    Repeater {
                                        model: viewModel ? viewModel.selectedKeywords : []
                                        delegate: Rectangle {
                                            readonly property real maxChipWidth: keywordFlow.width > 0 ? Math.min(keywordFlow.width, 180) : 180

                                            radius: 12
                                            color: Qt.rgba(0.31, 0.55, 1.0, 0.18)
                                            border.width: 1
                                            border.color: Qt.rgba(0.31, 0.55, 1.0, 0.32)
                                            implicitHeight: 28
                                            implicitWidth: Math.min(maxChipWidth, chipText.implicitWidth + 18)
                                            clip: true

                                            Text {
                                                id: chipText
                                                anchors.centerIn: parent
                                                width: parent.width - 16
                                                text: modelData
                                                color: Theme.text
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "解析状态：" + (viewModel ? viewModel.selectedAnalysisStatusLabel : "")
                                    color: Theme.muted
                                    wrapMode: Text.WrapAnywhere
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "确认状态：" + (viewModel ? viewModel.selectedConfirmationStatusLabel : "")
                                    color: Theme.muted
                                    wrapMode: Text.WrapAnywhere
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Rectangle {
                            id: pathCard

                            Layout.fillWidth: true
                            radius: 16
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.line
                            clip: true
                            implicitHeight: Math.max(120, pathColumn.implicitHeight + 28)

                            ColumnLayout {
                                id: pathColumn

                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8

                                Text {
                                    text: "路径与解析图片"
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "源文件：" + (viewModel ? viewModel.selectedFilePath : "")
                                    color: Theme.muted
                                    wrapMode: Text.WrapAnywhere
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "解析图片目录：" + (viewModel ? viewModel.selectedCachePath : "")
                                    color: Theme.muted
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }

                        Text {
                            text: "逐帧解析"
                            color: Theme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: viewModel && viewModel.hasSelection && viewModel.selectedFrameSearchStatus.length > 0
                            text: viewModel ? viewModel.selectedFrameSearchStatus : ""
                            color: Theme.muted
                            font.pixelSize: 12
                            wrapMode: Text.WrapAnywhere
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: viewModel
                                && viewModel.hasSelection
                                && !viewModel.selectedFramesLoading
                                && viewModel.selectedFrameCount > 0
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: !viewModel ? ""
                                    : (viewModel.selectedRemainingFrameCount > 0
                                        ? "当前显示 " + viewModel.selectedVisibleFrameCount + " / " + viewModel.selectedFrameCount + " 帧，剩余 " + viewModel.selectedRemainingFrameCount + " 帧"
                                        : "当前显示 " + viewModel.selectedVisibleFrameCount + " 帧")
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            ActionButton {
                                Layout.preferredWidth: 106
                                Layout.preferredHeight: 30
                                visible: viewModel && viewModel.canLoadMoreSelectedFrames
                                text: "再加载一批"
                                onClicked: if (viewModel) viewModel.loadMoreSelectedFrames()
                            }

                            ActionButton {
                                Layout.preferredWidth: 96
                                Layout.preferredHeight: 30
                                visible: viewModel && viewModel.canLoadMoreSelectedFrames
                                text: "全部展开"
                                onClicked: if (viewModel) viewModel.showAllSelectedFrames()
                            }

                            ActionButton {
                                Layout.preferredWidth: 84
                                Layout.preferredHeight: 30
                                visible: viewModel && viewModel.canExpandSelectedFrames && viewModel.selectedFramesExpanded
                                text: "收起"
                                onClicked: if (viewModel) viewModel.collapseSelectedFrames()
                            }
                        }

                        Repeater {
                            model: viewModel ? viewModel.selectedFrames : []

                            delegate: Rectangle {
                                id: frameCard

                                Layout.fillWidth: true
                                radius: 14
                                color: Theme.bg
                                border.width: 1
                                border.color: Theme.line
                                clip: true
                                implicitHeight: Math.max(132, frameTextColumn.implicitHeight + 20)

                                RowLayout {
                                    id: frameRow

                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: 120
                                        Layout.preferredHeight: 108
                                        radius: 12
                                        color: Theme.mediaSurface
                                        border.width: 1
                                        border.color: Theme.line
                                        clip: true

                                        Image {
                                            anchors.fill: parent
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            cache: true
                                            source: modelData.imagePath.length > 0 ? "file:///" + modelData.imagePath.replace(/\\/g, "/") : ""
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: modelData.imagePath.length === 0
                                            text: "暂无帧图"
                                            color: Theme.muted
                                            font.pixelSize: 11
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            enabled: modelData.imagePath.length > 0
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.openImageViewer(root.imageFileSource(modelData.imagePath))
                                        }
                                    }

                                    ColumnLayout {
                                        id: frameTextColumn

                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignTop
                                        spacing: 4

                                        Text {
                                            Layout.fillWidth: true
                                            text: "第 " + modelData.frameNumber + " 帧 · " + modelData.timestampLabel
                                            color: Theme.text
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.caption.length > 0 ? modelData.caption : (modelData.errorMessage.length > 0 ? modelData.errorMessage : "暂无描述")
                                            color: Theme.muted
                                            wrapMode: Text.WrapAnywhere
                                            maximumLineCount: 3
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.matchText.length > 0 ? "命中：" + modelData.matchText : ""
                                            color: Theme.blue
                                            visible: text.length > 0
                                            wrapMode: Text.WrapAnywhere
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.tags.length > 0 ? "标签：" + modelData.tags : ""
                                            color: Theme.muted
                                            visible: text.length > 0
                                            wrapMode: Text.WrapAnywhere
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.objects.length > 0 ? "对象：" + modelData.objects : ""
                                            color: Theme.muted
                                            visible: text.length > 0
                                            wrapMode: Text.WrapAnywhere
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.actions.length > 0 ? "动作：" + modelData.actions : ""
                                            color: Theme.muted
                                            visible: text.length > 0
                                            wrapMode: Text.WrapAnywhere
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.setting.length > 0 ? "场景：" + modelData.setting : ""
                                            color: Theme.muted
                                            visible: text.length > 0
                                            wrapMode: Text.WrapAnywhere
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: viewModel
                                && viewModel.hasSelection
                                && !viewModel.selectedFramesLoading
                                && viewModel.selectedFrames.length === 0
                            text: shellVm && shellVm.globalSearchText.length > 0
                                ? "当前关键词没有命中该视频的逐帧解析。"
                                : "当前还没有逐帧解析结果。"
                            color: Theme.muted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: batchAnalyzeDialog

        modal: true
        width: 520
        height: 220
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        padding: 0
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 18
            color: Theme.bg
            border.width: 1
            border.color: Theme.line
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: "当前结果中已有解析完成素材"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Black
            }

            Text {
                Layout.fillWidth: true
                text: "请选择只解析待解析和失败素材，或将当前搜索/筛选结果全部重新解析。"
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                ActionButton {
                    Layout.preferredWidth: 74
                    Layout.preferredHeight: 36
                    text: "取消"
                    onClicked: batchAnalyzeDialog.close()
                }

                ActionButton {
                    Layout.preferredWidth: 132
                    Layout.preferredHeight: 36
                    text: "解析未完成素材"
                    primary: true
                    onClicked: {
                        batchAnalyzeDialog.close()
                        if (viewModel) {
                            viewModel.analyzeVisiblePending()
                        }
                    }
                }

                ActionButton {
                    Layout.preferredWidth: 118
                    Layout.preferredHeight: 36
                    text: "全部重新解析"
                    onClicked: {
                        batchAnalyzeDialog.close()
                        if (viewModel) {
                            viewModel.analyzeVisibleAll()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: imageViewerOverlay
        anchors.fill: parent
        visible: root.imageViewerSource.length > 0
        z: 1000
        color: Qt.rgba(0, 0, 0, 0.92)
        focus: visible

        property real lastMouseX: 0
        property real lastMouseY: 0

        onVisibleChanged: if (visible) forceActiveFocus()
        Keys.onEscapePressed: root.closeImageViewer()

        Item {
            id: imageViewerViewport
            anchors.fill: parent
            anchors.margins: 24
            clip: true

            readonly property real fitScale: imageViewerImage.status === Image.Ready
                && imageViewerImage.implicitWidth > 0
                && imageViewerImage.implicitHeight > 0
                    ? Math.min(width / imageViewerImage.implicitWidth,
                               height / imageViewerImage.implicitHeight,
                               1.0)
                    : 1.0

            Image {
                id: imageViewerImage
                source: root.imageViewerSource
                asynchronous: true
                cache: false
                smooth: true
                width: implicitWidth * imageViewerViewport.fitScale * root.imageViewerScale
                height: implicitHeight * imageViewerViewport.fitScale * root.imageViewerScale
                x: (imageViewerViewport.width - width) / 2 + root.imageViewerOffsetX
                y: (imageViewerViewport.height - height) / 2 + root.imageViewerOffsetY
                fillMode: Image.Stretch
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                onPressed: function(mouse) {
                    imageViewerOverlay.forceActiveFocus()
                    imageViewerOverlay.lastMouseX = mouse.x
                    imageViewerOverlay.lastMouseY = mouse.y
                }
                onPositionChanged: function(mouse) {
                    if (!pressed) {
                        return
                    }
                    root.imageViewerOffsetX += mouse.x - imageViewerOverlay.lastMouseX
                    root.imageViewerOffsetY += mouse.y - imageViewerOverlay.lastMouseY
                    imageViewerOverlay.lastMouseX = mouse.x
                    imageViewerOverlay.lastMouseY = mouse.y
                }
                onDoubleClicked: {
                    root.imageViewerScale = 1.0
                    root.imageViewerOffsetX = 0
                    root.imageViewerOffsetY = 0
                }
                onWheel: function(wheel) {
                    imageViewerOverlay.forceActiveFocus()
                    var nextScale = root.imageViewerScale * (wheel.angleDelta.y > 0 ? 1.15 : 0.87)
                    root.imageViewerScale = Math.max(0.2, Math.min(12.0, nextScale))
                    wheel.accepted = true
                }
            }
        }

        Rectangle {
            width: 40
            height: 40
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 18
            radius: 20
            color: closeArea.pressed ? "#334155" : "#1F2937"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.18)

            Text {
                anchors.centerIn: parent
                text: "X"
                color: "white"
                font.pixelSize: 18
                font.weight: Font.Bold
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeImageViewer()
            }
        }
    }
}

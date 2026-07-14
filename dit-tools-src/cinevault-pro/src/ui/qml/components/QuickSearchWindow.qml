import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import CineVault

Window {
    id: root
    objectName: "quickSearchWindow"

    property var controller
    property var materialCenterViewModel
    property var shellViewModel
    property var mainWindow
    property int selectedFlatIndex: 0
    property point dragOrigin: Qt.point(0, 0)
    readonly property bool hasQuery: searchField.text.trim().length > 0
    readonly property int folderResultCount: hasQuery && materialCenterViewModel
        ? materialCenterViewModel.folderCount : 0
    readonly property int primaryResultCount: hasQuery && materialCenterViewModel
        ? (materialCenterViewModel.frameSearchMode
            ? materialCenterViewModel.frameCount
            : materialCenterViewModel.assetCount)
        : 0
    readonly property int totalResultCount: folderResultCount + primaryResultCount
    readonly property color quickBg: "#0E1014"
    readonly property color quickHeader: "#171A20"
    readonly property color quickSurface: "#20242C"
    readonly property color quickSurfaceHover: "#252B35"
    readonly property color quickLine: "#303640"
    readonly property color quickSelected: "#2A3039"
    readonly property color quickSelectedLine: "#4B5563"
    readonly property color quickText: "#F4F7FB"
    readonly property color quickMuted: "#A0A8B4"
    readonly property color quickWeak: "#737D8A"
    readonly property color quickAccent: "#C6CDD7"

    width: 820
    height: 650
    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    title: "影资管家快捷搜索"

    function imageSource(path) {
        return path && path.length > 0 && localImageUrlHelper
            ? localImageUrlHelper.sourceForInput(path)
            : ""
    }

    function openSearch() {
        selectedFlatIndex = 0
        searchField.text = ""
        if (controller) {
            var restoredPosition = controller.restoredWindowPosition(width, height)
            x = restoredPosition.x
            y = restoredPosition.y
        } else {
            x = Screen.virtualX + Math.round((Screen.width - width) / 2)
            y = Screen.virtualY + Math.round(Screen.height * 0.12)
        }
        show()
        raise()
        requestActivate()
        Qt.callLater(function() {
            searchField.forceActiveFocus()
            searchField.selectAll()
        })
    }

    function rememberPosition() {
        if (!controller) {
            return
        }
        var constrainedPosition = controller.rememberWindowPosition(x, y, width, height)
        x = constrainedPosition.x
        y = constrainedPosition.y
    }

    function hideSearch() {
        searchDebounce.stop()
        hide()
    }

    function clampSelection() {
        if (totalResultCount <= 0) {
            selectedFlatIndex = 0
        } else {
            selectedFlatIndex = Math.max(0, Math.min(selectedFlatIndex, totalResultCount - 1))
        }
        if (selectedFlatIndex < folderResultCount) {
            folderList.currentIndex = selectedFlatIndex
            resultList.currentIndex = -1
            folderList.positionViewAtIndex(folderList.currentIndex, ListView.Contain)
        } else {
            folderList.currentIndex = -1
            resultList.currentIndex = selectedFlatIndex - folderResultCount
            resultList.positionViewAtIndex(resultList.currentIndex, ListView.Contain)
        }
    }

    function moveSelection(delta) {
        if (totalResultCount <= 0) {
            return
        }
        selectedFlatIndex = (selectedFlatIndex + delta + totalResultCount) % totalResultCount
        clampSelection()
    }

    function showMainWindow() {
        hideSearch()
        if (!mainWindow) {
            return
        }
        mainWindow.showNormal()
        mainWindow.raise()
        mainWindow.requestActivate()
    }

    function enterMaterialCenter() {
        if (!shellViewModel) {
            showMainWindow()
            return
        }
        shellViewModel.enterProjectFromLibrary()
        shellViewModel.globalSearchText = searchField.text
        shellViewModel.currentWorkspace = shellViewModel.materialCenterWorkspaceId
        showMainWindow()
    }

    function activateCurrent(locateOnly) {
        clampSelection()
        if (totalResultCount <= 0) {
            showMainWindow()
            return
        }
        if (selectedFlatIndex < folderResultCount) {
            if (folderList.currentItem) {
                folderList.currentItem.activate(locateOnly)
            }
            return
        }
        if (resultList.currentItem) {
            resultList.currentItem.activate(locateOnly)
        }
    }

    onTotalResultCountChanged: clampSelection()
    onActiveChanged: if (visible && !active) focusLossTimer.restart()

    Timer {
        id: focusLossTimer
        interval: 350
        onTriggered: if (root.visible && !root.active) root.hideSearch()
    }

    Timer {
        id: searchDebounce
        interval: 80
        onTriggered: {
            root.selectedFlatIndex = 0
            if (root.materialCenterViewModel) {
                root.materialCenterViewModel.setSearchText(searchField.text)
            }
        }
    }

    Connections {
        target: root.controller
        function onQuickSearchRequested() { root.openSearch() }
        function onShowMainWindowRequested() { root.showMainWindow() }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.WindowShortcut
        onActivated: root.hideSearch()
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        anchors.margins: 8
        radius: 20
        color: root.quickBg
        border.width: 1
        border.color: root.quickLine

        Item {
            id: dragStrip
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 16
            z: 20

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 5
                width: 44
                height: 4
                radius: 2
                color: root.quickLine
            }

            DragHandler {
                id: windowDragHandler
                target: null
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.SizeAllCursor
                onActiveChanged: {
                    if (active) {
                        root.dragOrigin = Qt.point(root.x, root.y)
                    } else {
                        root.rememberPosition()
                    }
                }
                onTranslationChanged: {
                    if (!active) {
                        return
                    }
                    root.x = root.dragOrigin.x + translation.x
                    root.y = root.dragOrigin.y + translation.y
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 86
                radius: 20
                color: root.quickHeader

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 20
                    color: parent.color
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        radius: 14
                        color: root.quickSurface
                        border.width: 1
                        border.color: root.quickLine

                        Text {
                            anchors.centerIn: parent
                            text: "⌕"
                            color: root.quickAccent
                            font.pixelSize: 28
                            font.weight: Font.Bold
                        }
                    }

                    ThemedTextField {
                        id: searchField
                        objectName: "quickSearchField"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        font.pixelSize: 19
                        placeholderText: "描述你要找的画面、日期、文件夹或素材…"
                        color: root.quickText
                        placeholderTextColor: root.quickWeak
                        selectionColor: root.quickSelectedLine
                        selectedTextColor: root.quickText
                        background: Rectangle {
                            radius: 12
                            color: searchField.hovered ? root.quickSurfaceHover : root.quickSurface
                            border.width: 1
                            border.color: searchField.activeFocus
                                ? root.quickSelectedLine : root.quickLine
                        }
                        onTextEdited: searchDebounce.restart()
                        onAccepted: root.activateCurrent(false)
                        Keys.onDownPressed: function(event) {
                            root.moveSelection(1)
                            event.accepted = true
                        }
                        Keys.onUpPressed: function(event) {
                            root.moveSelection(-1)
                            event.accepted = true
                        }
                        Keys.onTabPressed: function(event) {
                            root.moveSelection(1)
                            event.accepted = true
                        }
                        Keys.onBacktabPressed: function(event) {
                            root.moveSelection(-1)
                            event.accepted = true
                        }
                        Keys.onPressed: function(event) {
                            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Return) {
                                root.activateCurrent(true)
                                event.accepted = true
                            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_R) {
                                searchDebounce.restart()
                                event.accepted = true
                            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_I) {
                                root.showMainWindow()
                                if (root.shellViewModel) root.shellViewModel.openSettings()
                                event.accepted = true
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: shortcutText.implicitWidth + 20
                        Layout.preferredHeight: 30
                        radius: 8
                        color: root.quickSurface
                        border.width: 1
                        border.color: root.quickLine

                        Text {
                            id: shortcutText
                            anchors.centerIn: parent
                            text: root.controller ? root.controller.shortcut : "Alt+Space"
                            color: root.quickMuted
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 10
                    anchors.bottomMargin: 8
                    spacing: 6

                    Text {
                        visible: !root.hasQuery
                        Layout.fillWidth: true
                        Layout.topMargin: 44
                        text: "输入自然语言开始全局素材搜索\n例如：上周三拍摄的红色汽车、海边日落的航拍画面"
                        color: root.quickMuted
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.55
                    }

                    RowLayout {
                        visible: root.hasQuery && root.folderResultCount > 0
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8

                        Text {
                            text: "文件夹"
                            color: root.quickText
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.folderResultCount + " 项"
                            color: root.quickWeak
                            font.pixelSize: 12
                        }
                    }

                    ListView {
                        id: folderList
                        visible: root.hasQuery && count > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(count * 58, 132)
                        clip: true
                        spacing: 4
                        model: root.materialCenterViewModel ? root.materialCenterViewModel.folderModel : null
                        currentIndex: root.folderResultCount > 0 ? 0 : -1

                        delegate: Rectangle {
                            id: folderDelegate
                            required property int index
                            required property string folderKey
                            required property string name
                            required property string relativePath
                            required property string absolutePath
                            required property string projectName
                            required property int recursiveFileCount
                            property string folderKeyValue: folderKey
                            property string folderNameValue: name.length > 0 ? name : "未命名文件夹"
                            property string folderPathValue: relativePath.length > 0 ? relativePath : absolutePath

                            width: ListView.view.width
                            height: 54
                            radius: 12
                            color: root.selectedFlatIndex === index ? root.quickSelected : "transparent"
                            border.width: root.selectedFlatIndex === index ? 1 : 0
                            border.color: root.quickSelectedLine

                            function activate(locateOnly) {
                                if (!root.materialCenterViewModel) return
                                if (locateOnly) {
                                    root.materialCenterViewModel.locateFolder(folderKeyValue)
                                    root.hideSearch()
                                } else {
                                    root.materialCenterViewModel.openFolderProject(folderKeyValue)
                                    root.enterMaterialCenter()
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12

                                Text {
                                    text: "▰"
                                    color: root.quickAccent
                                    font.pixelSize: 22
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: folderDelegate.folderNameValue
                                        color: root.quickText
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: folderDelegate.projectName + "  ·  " + folderDelegate.folderPathValue
                                        color: root.quickMuted
                                        font.pixelSize: 12
                                        elide: Text.ElideMiddle
                                    }
                                }
                                Text {
                                    text: folderDelegate.recursiveFileCount + " 个文件"
                                    color: root.quickWeak
                                    font.pixelSize: 12
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: root.selectedFlatIndex = index
                                onClicked: folderDelegate.activate(mouse.modifiers & Qt.ControlModifier)
                            }
                        }
                    }

                    RowLayout {
                        visible: root.hasQuery && root.primaryResultCount > 0
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8

                        Text {
                            text: root.materialCenterViewModel && root.materialCenterViewModel.frameSearchMode
                                ? "画面" : "素材"
                            color: root.quickText
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.primaryResultCount + " 项"
                            color: root.quickWeak
                            font.pixelSize: 12
                        }
                    }

                    ListView {
                        id: resultList
                        visible: root.hasQuery && count > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 5
                        currentIndex: root.folderResultCount === 0 && count > 0 ? 0 : -1
                        model: !root.materialCenterViewModel
                            ? null
                            : (root.materialCenterViewModel.frameSearchMode
                                ? root.materialCenterViewModel.frameModel
                                : root.materialCenterViewModel.assetModel)

                        delegate: Rectangle {
                            id: resultDelegate
                            required property int index
                            required property string videoKey
                            required property string fileName
                            required property string projectName
                            required property string sourceName
                            required property string relativePath
                            required property string assetTypeLabel
                            required property int resultRank
                            required property string quickPreviewPath
                            required property string quickDetail
                            required property string quickMeta
                            required property string quickReasons
                            property bool frameResult: root.materialCenterViewModel
                                && root.materialCenterViewModel.frameSearchMode
                            property string videoKeyValue: videoKey
                            property string fileNameValue: fileName.length > 0 ? fileName : "未命名素材"
                            property string previewPathValue: quickPreviewPath
                            readonly property int flatIndex: root.folderResultCount + index

                            width: ListView.view.width
                            height: 104
                            radius: 13
                            color: root.selectedFlatIndex === flatIndex ? root.quickSelected : "transparent"
                            border.width: root.selectedFlatIndex === flatIndex ? 1 : 0
                            border.color: root.quickSelectedLine

                            function activate(locateOnly) {
                                if (!root.materialCenterViewModel || videoKeyValue.length === 0) return
                                root.materialCenterViewModel.selectVideo(videoKeyValue)
                                if (locateOnly) {
                                    root.materialCenterViewModel.locateSelectedSource()
                                    root.hideSearch()
                                } else {
                                    if (root.materialCenterViewModel.openSelectedProject()) {
                                        root.enterMaterialCenter()
                                    } else {
                                        root.showMainWindow()
                                    }
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 12

                                Rectangle {
                                    Layout.preferredWidth: 124
                                    Layout.fillHeight: true
                                    radius: 10
                                    color: root.quickSurface
                                    clip: true

                                    Image {
                                        id: previewImage
                                        anchors.fill: parent
                                        source: root.imageSource(resultDelegate.previewPathValue)
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: true
                                        smooth: true
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: previewImage.status !== Image.Ready
                                        text: resultDelegate.assetTypeLabel.length > 0
                                            ? resultDelegate.assetTypeLabel
                                            : (resultDelegate.frameResult ? "画面" : "素材")
                                        color: root.quickWeak
                                        font.pixelSize: 12
                                    }

                                    Rectangle {
                                        visible: previewImage.status === Image.Ready
                                            && resultDelegate.frameResult
                                            && resultDelegate.quickMeta.length > 0
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 5
                                        width: previewTime.implicitWidth + 10
                                        height: 21
                                        radius: 6
                                        color: "#B80E1014"

                                        Text {
                                            id: previewTime
                                            anchors.centerIn: parent
                                            text: resultDelegate.quickMeta.split(" · ")[0]
                                            color: root.quickText
                                            font.pixelSize: 10
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: resultDelegate.fileNameValue
                                            color: root.quickText
                                            font.pixelSize: 15
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: "#" + resultDelegate.resultRank
                                            color: root.quickAccent
                                            font.pixelSize: 12
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: resultDelegate.projectName + "  ·  " + resultDelegate.sourceName
                                        color: root.quickMuted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: resultDelegate.quickDetail.length > 0
                                            ? resultDelegate.quickDetail
                                            : resultDelegate.relativePath
                                        color: root.quickText
                                        font.pixelSize: 13
                                        elide: Text.ElideRight
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Text {
                                            Layout.fillWidth: true
                                            text: resultDelegate.quickReasons.length > 0
                                                ? resultDelegate.quickMeta + "  ·  " + resultDelegate.quickReasons
                                                : resultDelegate.quickMeta
                                            color: root.quickWeak
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            visible: root.selectedFlatIndex === resultDelegate.flatIndex
                                            text: "打开详情  →"
                                            color: root.quickAccent
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: root.selectedFlatIndex = resultDelegate.flatIndex
                                onClicked: resultDelegate.activate(mouse.modifiers & Qt.ControlModifier)
                            }
                        }
                    }

                    Item {
                        visible: root.hasQuery && root.totalResultCount === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 40
                            spacing: 10
                            Text {
                                width: parent.width
                                text: root.materialCenterViewModel && root.materialCenterViewModel.searchAssistantBusy
                                    ? "正在理解并搜索…"
                                    : "没有找到匹配结果"
                                color: root.quickText
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                width: parent.width
                                text: root.materialCenterViewModel
                                    ? (root.materialCenterViewModel.searchEmptyReason || root.materialCenterViewModel.searchAssistantStatusText)
                                    : "搜索服务尚未初始化"
                                color: root.quickMuted
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 16

                            Text { text: "↑↓ / Tab 选择"; color: root.quickWeak; font.pixelSize: 11 }
                            Text { text: "Enter 打开"; color: root.quickWeak; font.pixelSize: 11 }
                            Text { text: "Ctrl+Enter 定位"; color: root.quickWeak; font.pixelSize: 11 }
                            Text { text: "Esc 收起"; color: root.quickWeak; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text {
                                visible: root.materialCenterViewModel && root.materialCenterViewModel.searchAssistantBusy
                                text: "视觉语言模型增强中…"
                                color: root.quickAccent
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}

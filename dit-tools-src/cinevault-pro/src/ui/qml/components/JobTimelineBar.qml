import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var viewModel

    color: "#10131A"
    border.width: 1
    border.color: Theme.line
    implicitHeight: 48

    Flickable {
        id: timelineFlick
        anchors.fill: parent
        anchors.margins: 10
        clip: true
        contentWidth: timelineRow.implicitWidth
        contentHeight: timelineRow.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        Row {
            id: timelineRow
            height: parent.height
            spacing: 10

            Repeater {
                model: viewModel.timelineItems
                delegate: Rectangle {
                    height: 28
                    width: Math.min(260, Math.max(160, detailText.implicitWidth + 24))
                    radius: 14
                    color: Theme.panel2
                    border.width: 1
                    border.color: Theme.line

                    Text {
                        id: detailText
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        text: modelData.title + " " + modelData.progress + "% · " + modelData.stateLabel
                        color: Theme.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}

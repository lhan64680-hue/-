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

    Row {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Repeater {
            model: viewModel.timelineItems
            delegate: Rectangle {
                height: 28
                width: Math.max(180, detailText.implicitWidth + 24)
                radius: 14
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line

                Text {
                    id: detailText
                    anchors.centerIn: parent
                    text: modelData.title + " " + modelData.progress + "% · " + modelData.stateLabel
                    color: Theme.text
                    font.pixelSize: 12
                }
            }
        }
    }
}

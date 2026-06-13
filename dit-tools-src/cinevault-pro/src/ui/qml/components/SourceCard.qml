import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property bool active: false
    property string name: ""
    property string metaText: ""
    property string statusLabel: ""
    property color statusColor: Theme.blue

    radius: 16
    color: Theme.panel2
    border.width: 1
    border.color: active ? Qt.rgba(0.31, 0.55, 1.0, 0.7) : Theme.line

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: name
            color: Theme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: metaText
            color: Theme.muted
            wrapMode: Text.Wrap
            font.pixelSize: 12
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        StatusChip {
            Layout.maximumWidth: parent.width
            label: statusLabel
            tint: statusColor
        }
    }
}

import QtQuick
import QtQuick.Controls
import CineVault

Rectangle {
    property string label: ""
    property color tint: Theme.blue

    implicitHeight: 28
    radius: 14
    color: Qt.rgba(tint.r, tint.g, tint.b, 0.16)
    border.width: 1
    border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.40)

    Text {
        anchors.centerIn: parent
        text: parent.label
        color: Theme.text
        font.pixelSize: 12
        font.weight: Font.Medium
    }
}

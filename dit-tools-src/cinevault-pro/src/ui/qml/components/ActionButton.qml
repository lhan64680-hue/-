import QtQuick
import QtQuick.Controls
import CineVault

Button {
    id: control

    property bool primary: false

    implicitWidth: Math.max(72, actionText.implicitWidth + 24)
    implicitHeight: 36
    padding: 0

    background: Rectangle {
        radius: 10
        color: !control.enabled
            ? Theme.panel2
            : (control.down
                ? (control.primary ? "#2F6FE0" : "#2A3242")
                : (control.hovered
                    ? (control.primary ? "#4F8CFF" : "#333D50")
                    : (control.primary ? "#3F7FF0" : Theme.card)))
        border.width: 1
        border.color: control.primary ? Qt.rgba(0.65, 0.78, 1.0, 0.45) : Theme.line
    }

    contentItem: Text {
        id: actionText
        text: control.text
        color: control.enabled ? Theme.text : Theme.weak
        font.pixelSize: 13
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

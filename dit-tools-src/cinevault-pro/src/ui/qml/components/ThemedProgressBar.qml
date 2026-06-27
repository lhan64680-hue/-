import QtQuick
import QtQuick.Controls
import CineVault

ProgressBar {
    id: control

    background: Rectangle {
        implicitHeight: 8
        radius: 4
        color: Theme.panel
        border.width: 1
        border.color: Theme.line
    }

    contentItem: Item {
        implicitHeight: 8

        Rectangle {
            width: control.indeterminate ? parent.width : control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: control.indeterminate ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.42) : Theme.blue
        }
    }
}

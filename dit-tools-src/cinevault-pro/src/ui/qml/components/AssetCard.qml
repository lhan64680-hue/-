import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property string title: ""
    property string subtitle: ""
    property string meta: ""
    property string tag: ""
    property bool selected: false

    radius: 18
    color: Theme.panel2
    border.width: 1
    border.color: selected ? Theme.blue : Theme.line

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 146
            radius: 18
            color: "#20273A"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.06)

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                radius: 12
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#35589E" }
                    GradientStop { position: 1.0; color: "#11151D" }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: title
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: subtitle
                color: Theme.muted
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: meta
                color: Theme.muted
                font.pixelSize: 12
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            StatusChip {
                Layout.maximumWidth: parent.width
                label: tag
                tint: Theme.green
            }
        }
    }
}

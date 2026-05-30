import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var viewModel

    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Text {
            text: "任务"
            color: Theme.text
            font.pixelSize: 28
            font.weight: Font.Black
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: viewModel.model

            delegate: Rectangle {
                width: ListView.view.width
                height: 84
                radius: 16
                color: Theme.panel2
                border.width: 1
                border.color: Theme.line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        text: model.title + " · " + model.stateLabel
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: model.detail
                        color: Theme.muted
                        wrapMode: Text.Wrap
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: model.progress
                    }
                }
            }
        }
    }
}

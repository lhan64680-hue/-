import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CineVault

Rectangle {
    property var viewModel

    color: Theme.panel
    border.width: 1
    border.color: Theme.line

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Text {
            Layout.fillWidth: true
            text: viewModel.title
            color: Theme.text
            font.pixelSize: 18
            font.weight: Font.Bold
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: viewModel.subtitle
            color: Theme.muted
            font.pixelSize: 13
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            radius: 18
            color: "#1B2231"
            border.width: 1
            border.color: Theme.line
        }

        ScrollView {
            id: detailScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Column {
                width: detailScroll.availableWidth
                spacing: 10

                Repeater {
                    model: viewModel.details
                    delegate: Rectangle {
                        width: parent.width
                        radius: 14
                        color: Theme.panel2
                        border.width: 1
                        border.color: Theme.line
                        implicitHeight: contentColumn.implicitHeight + 20

                        Column {
                            id: contentColumn
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            Text {
                                width: parent.width
                                text: modelData.label
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            Text {
                                width: parent.width
                                text: modelData.value
                                color: Theme.text
                                font.pixelSize: 13
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }
                }
            }
        }
    }
}

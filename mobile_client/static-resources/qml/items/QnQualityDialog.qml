import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

QnDialog {
    id: qualityDialog

    property alias resolutionList: repeater.model
    property string currentResolution

    signal qualityPicked(string resolution)

    title: qsTr("Video quality")

    Column {
        id: content

        property real selectedY: 0

        width: qualityDialog.width

        Repeater {
            id: repeater
            delegate: qualityItemDelegate
        }
    }

    Component {
        id: qualityItemDelegate


        Item {
            id: wrapper

            property bool current: modelData == currentResolution

            height: dp(56)
            width: content.width

            Rectangle {
                width: parent.width
                height: parent.height
                x: -dp(16)

                color: current ? QnTheme.dialogSelectedBackground : "transparent"

                QnMaterialSurface {
                    color: current ? "#30ffffff" : "#30000000"
                    onClicked: {
                        qualityDialog.hide()
                        qualityDialog.qualityPicked(modelData)
                    }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: modelData ? modelData : qsTr("Auto")
                color: current ? QnTheme.dialogSelectedText : QnTheme.dialogText
                font.pixelSize: sp(14)
            }

            onCurrentChanged: {
                if (current)
                    content.selectedY = y
            }

            onYChanged: {
                if (current)
                    content.selectedY = y
            }
        }
    }

    onShown: {
        ensureVisible(content.selectedY, content.selectedY + dp(56))
    }
}

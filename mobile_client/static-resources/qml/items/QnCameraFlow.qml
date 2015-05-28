import QtQuick 2.2
import Material 0.1

import "../main.js" as Main

Flickable {
    id: rootItem

    property alias model: repeater.model

    boundsBehavior: Flickable.StopAtBounds
    contentWidth: width
    contentHeight: flow.height

    Flow {
        id: flow

        spacing: Units.dp(8)
        width: rootItem.width

        Repeater {
            id: repeater

            Item {
                id: wrapper

                width: itemWidth - flow.spacing / 2
                height: itemHeight - flow.spacing / 2

                Image {
                    anchors.fill: parent
                    source: thumbnail
                    fillMode: Qt.KeepAspectRatio
                    sourceSize.width: wrapper.width
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: label.height + label.anchors.margins * 2
                    opacity: 0.7
                }

                Label {
                    id: label
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.margins: Units.dp(8)
                    text: resourceName
                }

                Ink {
                    anchors.fill: parent
                    onClicked: Main.openVideo(uuid)
                }

                onWidthChanged: console.log("change " + index + " w=" + width + " h=" + height)
            }
        }
    }

//    Scrollbar { flickableItem: rootItem }

    Timer {
        id: refreshTimer

        interval: 2 * 60 * 1000
        repeat: true
        running: true

        triggeredOnStart: true

        onTriggered: {
            model.refreshThumbnails(0, repeater.count)
        }
    }
}

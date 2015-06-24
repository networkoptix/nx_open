import QtQuick 2.2
import Material 0.1

import "../main.js" as Main

Flickable {
    id: cameraFlow

    property alias model: repeater.model

    contentWidth: width
    contentHeight: flow.height
    leftMargin: dp(16)
    rightMargin: dp(16)
    bottomMargin: dp(16)

    Flow {
        id: flow

        spacing: dp(16)
        width: cameraFlow.width - cameraFlow.leftMargin - cameraFlow.rightMargin

        Repeater {
            id: repeater

            QnCameraItem {
                text: model.resourceName
                status: model.resourceStatus
                thumbnail: model.thumbnail
                thumbnailWidth: flow.width / 2 - flow.spacing / 2

                onClicked: Main.openMediaResource(model.uuid)
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

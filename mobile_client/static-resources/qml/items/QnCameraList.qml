import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"

QnListView {
    id: camerasList

    clip: true

    leftMargin: dp(8)
    rightMargin: dp(8)
    topMargin: dp(8)
    bottomMargin: dp(8)
    spacing: dp(8)

    delegate: QnCameraListItem {
        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail

        onClicked: {
            var point = mapToItem(stackView, width / 2, height / 2)
            Main.openMediaResource(model.uuid, Math.max(0, point.x), Math.max(0, point.y), model.thumbnail)
        }
    }

    QnScrollIndicator {
        flickable: parent
    }
}

import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"

QnListView {
    id: camerasList

    leftMargin: dp(8)
    rightMargin: dp(8)
    topMargin: dp(8)
    bottomMargin: dp(8)
    spacing: dp(8)

    delegate: QnCameraListItem {
        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail

        onClicked: Main.openMediaResource(model.uuid)
    }

    QnScrollIndicator {
        flickable: parent
    }
}

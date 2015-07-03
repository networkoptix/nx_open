import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"

QnListView {
    id: camerasList

    leftMargin: dp(16)
    rightMargin: dp(16)
    topMargin: dp(16)
    bottomMargin: dp(16)
    spacing: dp(8)

    delegate: QnCameraListItem {
        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail

        onClicked: Main.openMediaResource(model.uuid)
    }
}

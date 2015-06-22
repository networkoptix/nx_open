import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"

QnPage {
    id: resourcesPage

    title: connectionManager.systemName

    leftToolBarComponent: QnMenuBackButton {

    }

    QnCameraListModel {
        id: camerasModel
    }

    QnCameraGrid {
        id: listView
        anchors.fill: parent
        model: camerasModel

        onVideoRequested: Main.openMediaResource(uuid)
    }

    onWidthChanged: updateLayout()

    Connections {
        target: connectionManager
        onInitialResourcesReceived: {
            updateLayout()
        }
    }

    function updateLayout() {
        camerasModel.updateLayout(resourcesPage.width, 3.0)
    }
}

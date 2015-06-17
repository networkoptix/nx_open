import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../controls"

QnPage {
    id: resourcesPage

    signal videoRequested(string uuid)

    title: qsTr("Resources")

    leftToolBarComponent: QnMenuBackButton {

    }

    QnCameraListModel {
        id: camerasModel
    }

    QnCameraGrid {
        id: listView
        anchors.fill: parent
        model: camerasModel

        onVideoRequested: resourcesPage.videoRequested(uuid)
    }

//    QnSideMenu {
//        id: sidebar
//        anchors.fill: parent
//        content: QnSideBar {
//            id: menuContent
//        }
//    }

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

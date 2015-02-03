import QtQuick 2.2
import Material 0.1
import com.networkoptix.qml 1.0

import "../items"

Page {
    id: resourcesDialog

    property bool showList: false

    title: qsTr("Resources")

    QnCameraListModel {
        id: camerasModel
    }

    Loader {
        id: listView
        anchors.fill: parent
        source: Qt.resolvedUrl(showList ?  "/qml/items/QnCameraList.qml" : "/qml/items/QnCameraGrid.qml")
        onLoaded: {
            item.model = camerasModel
        }
    }

    QnSideMenu {
        id: sidebar
        anchors.fill: parent
        content: Rectangle {
            color: "white"
        }
    }
}

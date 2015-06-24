import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"

QnPage {
    id: resourcesPage

    title: connectionManager.systemName

    rightToolBarComponent: rightComponent

    Component {
        id: rightComponent

        QnSearchItem {
            id: searchItem
        }
    }

    QnCameraListModel {
        id: camerasModel
    }

    QnCameraFlow {
        id: listView
        anchors.fill: parent
        model: camerasModel
    }

    Rectangle {
        id: loadingDummy
        anchors.fill: parent
        color: QnTheme.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Text {
            anchors.centerIn: parent
            text: connectionManager.connected ? qsTr("Loading...") : qsTr("Connecting...")
            font.pixelSize: sp(14)
            color: QnTheme.loadingText
        }
    }

    onWidthChanged: updateLayout()

    Connections {
        target: connectionManager
        onInitialResourcesReceived: {
            loadingDummy.opacity = 0.0
            updateLayout()
        }
        onConnectedChanged: {
            if (!connectionManager.connected) {
                loadingDummy.opacity = 1.0
            }
        }
    }

    function updateLayout() {
        camerasModel.updateLayout(resourcesPage.width, 3.0)
    }
}

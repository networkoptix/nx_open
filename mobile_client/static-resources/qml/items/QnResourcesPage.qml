import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"

QnPage {
    id: resourcesPage

    title: connectionManager.systemName

    property alias searchActive: searchItem.opened

    Connections {
        target: menuBackButton
        onClicked: {
            if (!resourcesPage.activePage || !menuBackButton.menuOpened)
                return

            searchItem.close()
        }
    }

    QnSearchItem {
        id: searchItem
        parent: toolBar
        onOpenedChanged: {
            searchListLoader.sourceComponent = opened ? searchListComponent : undefined
            if (opened) {
                menuBackButton.animateToBack()
                sideNavigation.enabled = false
            } else {
                menuBackButton.animateToMenu()
                sideNavigation.enabled = true
            }
        }
        visible: activePage
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

    Rectangle {
        id: searchList

        anchors.fill: parent
        color: QnTheme.windowBackground

        visible: searchListLoader.status == Loader.Ready && searchItem.text

        Loader {
            id: searchListLoader
            anchors.fill: parent
        }
    }

    Component {
        id: searchListComponent

        QnCameraList {
            id: cameraListItem
            model: QnCameraListModel {
                id: searchModel
            }

            Connections {
                target: searchItem
                onTextChanged: searchModel.setFilterFixedString(searchItem.text)
            }
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

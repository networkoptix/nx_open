import QtQuick 2.4
import QtQuick.Controls 1.3
import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"
import "../icons"

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
        parent: toolBar.contentItem
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
        visible: pageStatus == Stack.Active || pageStatus == Stack.Activating
        Keys.forwardTo: resourcesPage
    }

    QnCameraFlow {
        id: camerasList
        anchors.fill: parent
    }

    Rectangle {
        id: loadingDummy
        anchors.fill: parent
        color: QnTheme.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -dp(28)

            spacing: dp(16)

            QnCirclesPreloader {
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                height: dp(56)
                verticalAlignment: Qt.AlignVCenter

                text: connectionManager.connectionState == QnConnectionManager.Connecting ? qsTr("Connecting...") : qsTr("Loading...")
                font.pixelSize: sp(32)
                color: QnTheme.loadingText
            }
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

    Connections {
        target: connectionManager
        onInitialResourcesReceived: {
            loadingDummy.opacity = 0.0
        }
        onConnectionStateChanged: {
            if (connectionManager.connectionState == QnConnectionManager.Disconnected) {
                loadingDummy.opacity = 1.0
            }
        }
    }

    focus: true

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            if (searchItem.opened) {
                searchItem.close()
                event.accepted = true
            } else if (Main.backPressed()) {
                event.accepted = true
            }
        }
    }
}

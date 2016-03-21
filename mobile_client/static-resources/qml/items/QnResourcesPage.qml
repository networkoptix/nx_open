import QtQuick 2.4
import QtQuick.Controls 1.3
import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"
import "../icons"
import ".."

QnPage {
    id: resourcesPage

    title: mainWindow.currentSystemName

    property alias searchActive: searchItem.opened

    QnObject {
        id: d

        readonly property bool serverOffline: connectionManager.connectionState === QnConnectionManager.Connecting && !loadingDummy.visible
        property bool serverOfflineWarningVisible: false

        onServerOfflineChanged: {
            if (!serverOffline)
                serverOfflineWarningVisible = false
        }

        Timer {
            id: offlineWarningDelay

            interval: 20 * 1000
            repeat: false
            running: d.serverOffline

            onTriggered: {
                searchItem.close()
                d.serverOfflineWarningVisible = true
            }
        }
    }

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
        visible: (pageStatus == Stack.Active || pageStatus == Stack.Activating)
        Keys.forwardTo: resourcesPage

        enabled: !d.serverOfflineWarningVisible && !loadingDummy.visible
        opacity: !d.serverOfflineWarningVisible ? 1.0 : 0.2
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    QnCameraFlow {
        id: camerasList
        anchors.fill: parent
        anchors.topMargin: offlineWarning.height
        animationsEnabled: !loadingDummy.visible
    }

    Rectangle {
        id: offlineWarning

        width: parent.width
        height: d.serverOfflineWarningVisible ? dp(40) : 0

        clip: true
        anchors.horizontalCenter: parent.horizontalCenter

        color: QnTheme.attentionBackground

        Behavior on height {
            enabled: activePage
            NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
        }

        Text {
            id: warningText
            anchors.horizontalCenter: parent.horizontalCenter
            // Two lines below are the hack to prevent text from moving when the header changes its size
            anchors.verticalCenter: parent.bottom
            anchors.verticalCenterOffset: -dp(20)
            font.pixelSize: sp(16)
            font.weight: Font.DemiBold
            text: qsTr("Server offline")
            color: QnTheme.windowText
        }
    }

    Rectangle {
        id: offlineDimmer
        anchors.fill: parent
        anchors.topMargin: offlineWarning.height
        color: QnTheme.offlineDimmer

        Behavior on opacity { NumberAnimation { duration: 200 } }
        opacity: d.serverOfflineWarningVisible ? 1.0 : 0.0

        visible: opacity > 0

        MouseArea {
            anchors.fill: parent
            // Block mouse events
        }
    }

    Rectangle {
        id: loadingDummy
        anchors.fill: parent
        color: QnTheme.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }
        visible: opacity > 0

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

                text: connectionManager.connectionState === QnConnectionManager.Connected ? qsTr("Loading...") : qsTr("Connecting...")
                font.pixelSize: sp(32)
                color: QnTheme.loadingText
            }
        }

        onVisibleChanged:
        {
            if (!visible)
                mainWindow.unlockScreenOrientation()
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
            if (connectionManager.connectionState === QnConnectionManager.Disconnected) {
                loadingDummy.opacity = 1.0
            }
        }
    }

    focus: true

    Keys.onReleased: {
        if (Main.keyIsBack(event.key)) {
            if (searchItem.opened) {
                searchItem.close()
                event.accepted = true
            } else if (Main.backPressed()) {
                event.accepted = true
            }
        }
    }
}

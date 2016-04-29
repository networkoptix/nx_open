import QtQuick 2.4
import QtQuick.Layouts 1.3
import Qt.labs.controls 1.0
import com.networkoptix.qml 1.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import "../main.js" as Main
import "../controls"
import "../icons"
import ".."

Page
{
    id: resourcesPage

    Object
    {
        id: d

        readonly property bool serverOffline: connectionManager.connectionState === QnConnectionManager.Connecting && !loadingDummy.visible
        property bool serverOfflineWarningVisible: false

        onServerOfflineChanged:
        {
            if (!serverOffline)
                serverOfflineWarningVisible = false
        }

        Timer {
            id: offlineWarningDelay

            interval: 20 * 1000
            repeat: false
            running: d.serverOffline

            onTriggered:
            {
                searchToolBar.close()
                d.serverOfflineWarningVisible = true
            }
        }
    }

    title: mainWindow.currentSystemName
    leftButtonIcon: "/images/menu.png"
    onLeftButtonClicked: sideNavigation.open()
    titleControls:
    [
        QnIconButton
        {
            icon: "/images/search.png"
            enabled: !d.serverOfflineWarningVisible && !loadingDummy.visible
            opacity: !d.serverOfflineWarningVisible ? 1.0 : 0.2
            onClicked: searchToolBar.open()
        }
    ]

    SearchToolBar
    {
        id: searchToolBar
        parent: header
    }

    SideNavigation
    {
        id: sideNavigation
    }

    Rectangle
    {
        id: offlineWarning

        width: parent.width
        height: d.serverOfflineWarningVisible ? 40 : 0

        clip: true
        anchors.horizontalCenter: parent.horizontalCenter

        color: ColorTheme.orange_main

        Behavior on height
        {
            enabled: activePage
            NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
        }

        Text
        {
            id: warningText
            anchors.horizontalCenter: parent.horizontalCenter
            // Two lines below are the hack to prevent text from moving when the header changes its size
            anchors.verticalCenter: parent.bottom
            anchors.verticalCenterOffset: -20
            font.pixelSize: 16
            font.weight: Font.DemiBold
            text: qsTr("Server offline")
            color: ColorTheme.windowText
        }
    }

    QnCameraFlow
    {
        id: camerasList
        anchors.fill: parent
        anchors.topMargin: offlineWarning.height
        animationsEnabled: !loadingDummy.visible
    }

    Loader
    {
        id: searchListLoader
        anchors.fill: parent
        active: searchToolBar.text && searchToolBar.opacity == 1.0
        sourceComponent: searchListComponent
    }

    Rectangle
    {
        id: offlineDimmer
        anchors.fill: parent
        anchors.topMargin: offlineWarning.height
        color: QnTheme.offlineDimmer

        Behavior on opacity { NumberAnimation { duration: 200 } }
        opacity: d.serverOfflineWarningVisible ? 1.0 : 0.0

        visible: opacity > 0

        MouseArea
        {
            anchors.fill: parent
            // Block mouse events
        }
    }


    Rectangle
    {
        id: loadingDummy

        anchors.fill: parent
        color: QnTheme.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }
        visible: opacity > 0

        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -28

            spacing: 16

            QnCirclesPreloader {
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                height: 56
                verticalAlignment: Qt.AlignVCenter

                text: connectionManager.connectionState === QnConnectionManager.Connected ? qsTr("Loading...") : qsTr("Connecting...")
                font.pixelSize: 32
                color: QnTheme.loadingText
            }
        }

        onVisibleChanged:
        {
            if (!visible)
                mainWindow.unlockScreenOrientation()
        }
    }

    Component
    {
        id: searchListComponent

        Rectangle
        {
            color: ColorTheme.windowBackground

            QnCameraList
            {
                anchors.fill: parent
                model: QnCameraListModel { id: searchModel }
                Connections
                {
                    target: searchToolBar
                    onTextChanged: searchModel.setFilterFixedString(searchToolBar.text)
                }
            }
        }
    }

    Connections {
        target: connectionManager
        onInitialResourcesReceived:
        {
            loadingDummy.opacity = 0.0
        }
        onConnectionStateChanged:
        {
            if (connectionManager.connectionState === QnConnectionManager.Disconnected)
            {
                loadingDummy.opacity = 1.0
            }
        }
    }

    focus: true

    Keys.onReleased:
    {
        if (Main.keyIsBack(event.key))
        {
            if (searchToolBar.visible)
            {
                searchToolBar.close()
                event.accepted = true
            }
            else if (Main.backPressed())
            {
                event.accepted = true
            }
        }
        else if (event.key == Qt.Key_F2)
        {
            sideNavigation.open = !sideNavigation.open
        }
    }
}

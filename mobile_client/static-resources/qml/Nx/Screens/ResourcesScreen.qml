import QtQuick 2.0
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

import "private/ResourcesScreen"

Page
{
    id: resourcesScreen
    objectName: "resourcesScreen"

    leftButtonIcon: "/images/menu.png"
    onLeftButtonClicked: sideNavigation.open()
    warningText: qsTr("Server offline")

    titleControls:
    [
        IconButton
        {
            icon: "/images/search.png"
            enabled: d.enabled
            opacity: !warningVisible ? 1.0 : 0.2
            onClicked: searchToolBar.open()
            alwaysCompleteHighlightAnimation: false
        }
    ]

    QtObject
    {
        id: d

        readonly property bool serverOffline:
                connectionManager.connectionState === QnConnectionManager.Connecting &&
                !loadingDummy.visible
        readonly property bool enabled: !warningVisible && !loadingDummy.visible

        onServerOfflineChanged:
        {
            if (serverOffline)
                offlineWarningDelay.restart()
            else
                warningVisible = false
        }

    }

    Timer
    {
        id: offlineWarningDelay

        interval: 20 * 1000
        onTriggered:
        {
            searchToolBar.close()
            warningVisible = true
        }
    }

    SearchToolBar
    {
        id: searchToolBar
        parent: header
    }

    CamerasGrid
    {
        id: camerasGrid

        anchors
        {
            fill: parent
            topMargin: 12
            bottomMargin: 12
            leftMargin: 12
            rightMargin: 12
        }
        displayMarginBeginning: anchors.topMargin
        displayMarginEnd: anchors.bottomMargin
        enabled: d.enabled
    }

    Loader
    {
        id: searchListLoader
        anchors.fill: parent
        active: searchToolBar.text && searchToolBar.opacity == 1.0
        sourceComponent: searchListComponent
        enabled: d.enabled
    }

    Component
    {
        id: searchListComponent

        Rectangle
        {
            color: ColorTheme.windowBackground

            CamerasList
            {
                id: camerasList

                anchors.fill: parent
                anchors.margins: 16
                displayMarginBeginning: 16
                displayMarginEnd: 16

                Connections
                {
                    target: searchToolBar
                    onTextChanged: camerasList.model.setFilterFixedString(searchToolBar.text)
                }
            }
        }
    }

    Rectangle
    {
        id: offlineDimmer

        anchors.fill: parent
        color: ColorTheme.transparent(ColorTheme.base5, 0.8)

        Behavior on opacity { NumberAnimation { duration: 200 } }
        opacity: warningVisible ? 1.0 : 0.0
        visible: opacity > 0
    }

    Rectangle
    {
        id: loadingDummy

        anchors.fill: parent
        color: ColorTheme.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }
        visible: opacity > 0

        Column
        {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -28

            spacing: 16

            CirclesBusyIndicator
            {
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                height: 56
                verticalAlignment: Qt.AlignVCenter

                text: connectionManager.connectionState === QnConnectionManager.Connected
                        ? qsTr("Loading...") : qsTr("Connecting...")
                font.pixelSize: 32
                color: ColorTheme.base13
            }
        }

        onVisibleChanged:
        {
            if (!visible)
                mainWindow.unlockScreenOrientation()
        }
    }

    Connections
    {
        target: connectionManager

        onConnectionStateChanged:
        {
            if (connectionManager.connectionState === QnConnectionManager.Connected)
            {
                savedSessionsModel.updateSession(
                            currentSessionId,
                            connectionManager.currentUrl,
                            connectionManager.systemName,
                            true)
                loginSessionManager.lastUsedSessionId = currentSessionId
            }
            else if (connectionManager.connectionState === QnConnectionManager.Disconnected)
            {
                loadingDummy.opacity = 1
            }
        }

        onConnectionFailed:
        {
            var sessionId = currentSessionId
            currentSessionId = ""
            Workflow.openFailedSessionScreen(
                        sessionId,
                        title,
                        connectionManager.currentHost,
                        connectionManager.currentPort,
                        connectionManager.currentLogin,
                        connectionManager.currentPassword,
                        status,
                        infoParameter)
        }

        onInitialResourcesReceived:
        {
            loadingDummy.opacity = 0
        }
    }
}

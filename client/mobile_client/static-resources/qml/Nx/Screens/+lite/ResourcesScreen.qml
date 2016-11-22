import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private/ResourcesScreen"

Page
{
    id: resourcesScreen
    objectName: "resourcesScreen"

    toolBar.visible: false
    warningText: qsTr("Server offline")

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
            {
                offlineWarningDelay.restart()
            }
            else
            {
                warningVisible = false
                offlineWarningDelay.stop()
            }
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

    CamerasGrid
    {
        id: camerasGrid
        objectName: "camerasGrid"

        anchors.fill: parent
        enabled: d.enabled

        layoutId: uiController.layoutId
    }

    HelpPopup
    {
        id: helpPopup
        width: parent.width
        height: parent.height
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
        opacity: 0.0

        Column
        {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -28

            spacing: 16

            CirclesBusyIndicator
            {
                running: loadingDummy.visible
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
    }

    Connections
    {
        target: connectionManager

        onConnectionStateChanged:
        {
            if (connectionManager.connectionState === QnConnectionManager.Disconnected)
            {
                loadingDummy.opacity = 1
            }
        }

        onConnectionFailed:
        {
            var systemName = title ? title : getLastUsedSystemName()
            Workflow.openSessionsScreenWithWarning(
                connectionManager.connectionType == QnConnectionManager.LiteClientConnection
                    ? "" : systemName)
        }

        onInitialResourcesReceivedChanged:
        {
            if (connectionManager.initialResourcesReceived)
            {
                loadingDummy.opacity = 0
                autoLoginEnabled = true
            }
        }
    }

    Component.onCompleted:
    {
        if (!connectionManager.online || !connectionManager.initialResourcesReceived)
            loadingDummy.opacity = 1
    }

    Keys.onPressed:
    {
        if (loadingDummy.visible)
            return

        if (event.key == Qt.Key_F1)
        {
            helpPopup.open()
            helpPopup.contentItem.forceActiveFocus()
            event.accepted = true
        }
    }
}

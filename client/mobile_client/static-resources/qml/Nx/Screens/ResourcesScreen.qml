import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private/ResourcesScreen"

Page
{
    id: resourcesScreen
    objectName: "resourcesScreen"

    leftButtonIcon: lp("/images/menu.png")
    onLeftButtonClicked: sideNavigation.open()
    warningText: qsTr("Server offline")
    sideNavigationEnabled: !searchToolBar.visible

    titleControls:
    [
        IconButton
        {
            icon: lp("/images/search.png")
            enabled: d.enabled
            opacity: !warningVisible ? 1.0 : 0.2
            onClicked:
            {
                sideNavigation.close()
                searchToolBar.open()
            }
            alwaysCompleteHighlightAnimation: false
            visible: !liteMode
        }
    ]

    LayoutAccessor
    {
        id: layout
        layoutId: uiController.layoutId
    }

    Binding
    {
        target: resourcesScreen
        property: "title"
        value: layout.name || connectionManager.systemName
        when: connectionManager.online
    }

    QtObject
    {
        id: d

        readonly property bool serverOffline:
            connectionManager.connectionState === QnConnectionManager.Reconnecting
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

    SearchToolBar
    {
        id: searchToolBar
        parent: toolBar
    }

    CamerasGrid
    {
        id: camerasGrid
        objectName: "camerasGrid"

        anchors
        {
            fill: parent
            topMargin: 4
            bottomMargin: 4
            leftMargin: 4
            rightMargin: 4
        }
        displayMarginBeginning: anchors.topMargin
        displayMarginEnd: anchors.bottomMargin
        enabled: d.enabled

        layoutId: uiController.layoutId

        keepStatuses: !resourcesScreen.warningVisible
            && connectionManager.connectionState !== QnConnectionManager.Ready

        active: activePage

        ScrollIndicator.vertical: ScrollIndicator
        {
            leftPadding: 2
            width: 4
        }
    }

    DummyMessage
    {
        anchors.fill: parent
        title: qsTr("No cameras available on this layout")
        buttonText: qsTr("Show all cameras")
        onButtonClicked: uiController.layoutId = ""
        visible: camerasGrid.count == 0
                 && uiController.layoutId != ""
                 && connectionManager.online
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
                anchors.margins: 8
                displayMarginBeginning: 8
                displayMarginEnd: 8

                Connections
                {
                    target: searchToolBar
                    onTextChanged: camerasList.model.setFilterFixedString(searchToolBar.text)
                }

                ScrollIndicator.vertical: ScrollIndicator
                {
                    leftPadding: 4
                    width: 4
                }
            }

            DummyMessage
            {
                anchors.fill: parent
                title: qsTr("Nothing found")
                visible: camerasList.count == 0
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
        opacity: connectionManager.online ? 0.0 : 1.0

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
                topPadding: 26
                text: connectionManager.connectionState === QnConnectionManager.Connected
                    ? qsTr("Loading...") : qsTr("Connecting...")
                font.pixelSize: 22
                color: ColorTheme.contrast16
            }
        }
    }

    Connections
    {
        target: connectionManager

        onConnectionFailed:
        {
            var systemName = title ? title : getLastUsedSystemName()
            Workflow.openSessionsScreenWithWarning(
                connectionManager.connectionType == QnConnectionManager.LiteClientConnection
                    ? "" : systemName)
        }
    }
}

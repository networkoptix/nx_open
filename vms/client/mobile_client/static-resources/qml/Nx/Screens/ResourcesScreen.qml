import QtQuick 2.6
import QtQuick.Controls 2.4
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

    leftButtonIcon.source: lp("/images/menu.png")
    onLeftButtonClicked: sideNavigation.open()
    sideNavigationEnabled: !searchToolBar.visible
    property alias filterIds: camerasGrid.filterIds

    titleControls:
    [
        IconButton
        {
            padding: 0
            icon.source: lp("/images/search.png")
            enabled: d.enabled
            opacity: !connectionManager.restoringConnection ? 1.0 : 0.2
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

        readonly property bool enabled: !connectionManager.restoringConnection
            && !loadingDummy.visible
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

        keepStatuses: !connectionManager.restoringConnection
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
            property string filterString: searchToolBar.text
            onFilterStringChanged: camerasList.model.setFilterFixedString(filterString)

            color: ColorTheme.windowBackground

            CamerasList
            {
                id: camerasList

                anchors.fill: parent
                anchors.margins: 8
                displayMarginBeginning: 8
                displayMarginEnd: 8

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
        opacity: connectionManager.restoringConnection ? 1.0 : 0.0
        visible: opacity > 0
    }

    Rectangle
    {
        id: loadingDummy

        anchors.fill: parent
        color: ColorTheme.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }
        visible: opacity > 0
        opacity: connectionManager.online || connectionManager.restoringConnection ? 0.0 : 1.0

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
            if (!resourcesScreen.activePage)
                return

            var systemName = title ? title : getLastUsedSystemName()
            Workflow.openSessionsScreenWithWarning(
                connectionManager.connectionType == QnConnectionManager.LiteClientConnection
                    ? "" : systemName)
        }
    }
}

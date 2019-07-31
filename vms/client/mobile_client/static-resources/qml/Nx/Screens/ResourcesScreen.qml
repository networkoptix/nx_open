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
            enabled: camerasGrid.enabled
            opacity: ConnectionController.reconnecting ? 0.2 : 1.0
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
        when: ConnectionController.online
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
        enabled: !ConnectionController.reconnecting && !loadingDummy.visible

        layoutId: uiController.layoutId

        keepStatuses: !ConnectionController.reconnecting && !ConnectionController.ready

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
                 && ConnectionController.online
    }

    Loader
    {
        id: searchListLoader
        anchors.fill: parent
        active: searchToolBar.text && searchToolBar.opacity == 1.0
        sourceComponent: searchListComponent
        enabled: camerasGrid.enabled
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
        opacity: ConnectionController.reconnecting ? 1.0 : 0.0
        visible: opacity > 0
    }

    Rectangle
    {
        id: loadingDummy

        anchors.fill: parent
        color: ColorTheme.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }
        visible: opacity > 0
        opacity: ConnectionController.online || ConnectionController.reconnecting ? 0.0 : 1.0

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
                text: ConnectionController.connected
                    ? qsTr("Loading...") : qsTr("Connecting...")
                font.pixelSize: 22
                color: ColorTheme.contrast16
            }
        }
    }
}

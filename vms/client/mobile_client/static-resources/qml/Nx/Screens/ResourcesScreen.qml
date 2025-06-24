// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Ui.Sheets
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

import "private/ResourcesScreen"

Page
{
    id: resourcesScreen
    objectName: "resourcesScreen"

    leftButtonIcon.source: "image://skin/20x20/Solid/layouts.svg?primary=light10"
    leftButtonIcon.width: 20
    leftButtonIcon.height: 20
    leftButtonEnabled: !loadingDummy.visible

    LayoutSheet
    {
        id: layoutSheet
    }

    onLeftButtonClicked: layoutSheet.open()
    property alias filterIds: camerasGrid.filterIds

    rightControl: IconButton
    {
        id: searchButton

        anchors.centerIn: parent

        padding: 0
        icon.source: "image://skin/24x24/Outline/search.svg?primary=light10"
        icon.width: 24
        icon.height: 24
        enabled: camerasGrid.enabled
        opacity: windowContext.sessionManager.hasReconnectingSession ? 0.2 : 1.0
        onClicked: searchToolBar.open()
        alwaysCompleteHighlightAnimation: false
    }

    ResourceHelper
    {
        id: layout
        resource: windowContext.depricatedUiController.layout

        onResourceChanged:
        {
            if (resource && (resource.flags & ResourceFlag.cross_system))
                resource.makeSystemConnectionsWithUserInteraction()
        }
    }

    Binding
    {
        target: resourcesScreen
        property: "title"
        value: layout.resourceName || windowContext.sessionManager.systemName
        when: windowContext.sessionManager.hasActiveSession
        restoreMode: Binding.RestoreBinding
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
            topMargin: 4 - camerasGrid.spacing / 2
            bottomMargin: 4 - camerasGrid.spacing / 2
            leftMargin: 4 - camerasGrid.spacing / 2
            rightMargin: 4 - camerasGrid.spacing / 2
        }

        enabled: !windowContext.sessionManager.hasReconnectingSession && !loadingDummy.visible

        layout: windowContext.depricatedUiController.layout

        keepStatuses: !windowContext.sessionManager.hasReconnectingSession && !windowContext.sessionManager.hasConnectedSession

        active: activePage

        onOpenVideoScreen:
            (resource, thumbnailUrl, camerasModel) =>
                handleOpenVideoScreenRequest(resource, thumbnailUrl, camerasModel)
    }

    DummyMessage
    {
        anchors.fill: parent
        title: qsTr("No cameras available on this layout")
        buttonText: qsTr("Show all cameras")
        onButtonClicked: windowContext.depricatedUiController.layout = null
        visible: camerasGrid.count == 0
                 && windowContext.depricatedUiController.layout
                 && windowContext.sessionManager.hasActiveSession
    }

    Loader
    {
        id: searchListLoader
        anchors.fill: parent
        active: searchToolBar.text && searchToolBar.opacity == 1.0
        sourceComponent: searchListComponent
        enabled: camerasGrid.enabled
    }

    function handleOpenVideoScreenRequest(resource, thumbnailUrl, camerasModel)
    {
        for (var i = 0; i !== camerasGrid.count; ++i)
        {
            const cameraItem = camerasGrid.itemAtIndex(i)
            if (cameraItem && cameraItem.mediaPlayer)
                cameraItem.mediaPlayer.stop()
        }

        const point = mapToItem(camerasGrid.parent, width / 2, height / 2)
        Workflow.openVideoScreen(resource, thumbnailUrl, Math.max(0, point.x),
            Math.max(0, point.y), undefined, camerasModel)
    }

    Component
    {
        id: searchListComponent

        Rectangle
        {
            property string filterRegExp:
                NxGlobals.makeSearchRegExp(`*${searchToolBar.text}*`)
            onFilterRegExpChanged: camerasList.model.setFilterRegularExpression(filterRegExp)

            color: ColorTheme.colors.dark4

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

                onOpenVideoScreen:
                    (resource, thumbnailUrl, camerasModel) =>
                    {
                        handleOpenVideoScreenRequest(resource, thumbnailUrl, camerasModel)
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
        color: ColorTheme.transparent(ColorTheme.colors.dark5, 0.8)

        Behavior on opacity { NumberAnimation { duration: 200 } }
        opacity: windowContext.sessionManager.hasReconnectingSession ? 1.0 : 0.0
        visible: opacity > 0
    }

    Rectangle
    {
        id: loadingDummy

        anchors.fill: parent
        color: ColorTheme.colors.dark4
        Behavior on opacity { NumberAnimation { duration: 200 } }
        visible: opacity > 0
        opacity: windowContext.sessionManager.hasActiveSession ? 0.0 : 1.0

        Column
        {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -28

            spacing: 16

            CirclesBusyIndicator
            {
                running: loadingDummy.visible
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            }

            Text
            {
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                topPadding: 26
                text: windowContext.sessionManager.hasConnectingSession
                    ? qsTr("Connecting...")
                    : qsTr("Loading...")
                font.pixelSize: 22
                color: ColorTheme.colors.light16
            }
        }
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core
import Nx.Items
import Nx.Mobile
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

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
            opacity: sessionManager.hasReconnectingSession ? 0.2 : 1.0
            onClicked:
            {
                sideNavigation.close()
                searchToolBar.open()
            }
            alwaysCompleteHighlightAnimation: false
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
        value: layout.name || sessionManager.systemName
        when: sessionManager.hasActiveSession
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
            topMargin: 4
            bottomMargin: 4
            leftMargin: 4
            rightMargin: 4
        }

        enabled: !sessionManager.hasReconnectingSession && !loadingDummy.visible

        layoutId: uiController.layoutId

        keepStatuses: !sessionManager.hasReconnectingSession && !sessionManager.hasConnectedSession

        active: activePage

        ScrollIndicator.vertical: ScrollIndicator
        {
            leftPadding: 2
            width: 4
        }

        onOpenVideoScreen:
            (resource, thumbnailUrl, camerasModel) =>
                handleOpenVideoScreenRequest(resource, thumbnailUrl, camerasModel)
    }

    DummyMessage
    {
        anchors.fill: parent
        title: qsTr("No cameras available on this layout")
        buttonText: qsTr("Show all cameras")
        onButtonClicked: uiController.layoutId = ""
        visible: camerasGrid.count == 0
                 && uiController.layoutId != ""
                 && sessionManager.hasActiveSession
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

            color: ColorTheme.colors.windowBackground

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
        opacity: sessionManager.hasReconnectingSession ? 1.0 : 0.0
        visible: opacity > 0
    }

    Rectangle
    {
        id: loadingDummy

        anchors.fill: parent
        color: ColorTheme.colors.windowBackground
        Behavior on opacity { NumberAnimation { duration: 200 } }
        visible: opacity > 0
        opacity: sessionManager.hasActiveSession ? 0.0 : 1.0

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
                text: sessionManager.hasConnectingSession
                    ? qsTr("Connecting...")
                    : qsTr("Loading...")
                font.pixelSize: 22
                color: ColorTheme.colors.light16
            }
        }
    }
}

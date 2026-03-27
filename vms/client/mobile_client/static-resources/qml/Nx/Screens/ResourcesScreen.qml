// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Controls
import Nx.Common
import Nx.Core
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Mobile.Ui.Sheets
import Nx.Screens
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile
import nx.vms.client.mobile.timeline as Timeline

import "private/ResourcesScreen"

AdaptiveScreen
{
    id: resourcesScreen

    objectName: "resourcesScreen"

    property alias filterIds: camerasGrid.filterIds

    toolBar.controls:
        [
            LayoutItemProxy
            {
                anchors.verticalCenter: parent.verticalCenter
                visible: videoScreenLoader.item
                    && (videoScreenLoader.item.selectedObjectsType == Timeline.ObjectsLoader.ObjectsType.motion
                        || videoScreenLoader.item.selectedObjectsType == Timeline.ObjectsLoader.ObjectsType.analytics)
                    && videoScreenLoader.item.customRoiExists

                target: contentItem === videoScreenLoader.item ? videoScreenLoader.item.motionAreaButton : null
            }
        ]

    contentItem: resourceHelper.isCamera ? videoScreenLoader.item : camerasGrid
    overlayItem: overlayItem
    longContent: contentItem === videoScreenLoader.item

    customLeftControl: ToolBarButton
    {
        id: leftControl

        anchors.centerIn: parent
        visible: state !== ""
        states:
        [
            State
            {
                name: "returnToLayout"
                when: resourcesScreen.contentItem === videoScreenLoader.item
                    && (resourceHelper.isLayout
                        || resourceHelper.resource === null
                        || (resourceHelper.isCamera && !LayoutController.isTabletLayout))

                PropertyChanges
                {
                    leftControl.icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=light4"
                    leftControl.onClicked:
                    {
                        resourcesScreen.filterIds = []
                        videoScreenLoader.item.controller.stop()
                        windowContext.deprecatedUiController.resource = null
                        resourcesScreen.contentItem = camerasGrid
                    }
                }
            },
            State
            {
                name: "openResourceTreeSplash"
                when: !LayoutController.isTabletLayout

                PropertyChanges
                {
                    leftControl.icon.source: "image://skin/24x24/Outline/resource_tree.svg?primary=light4"
                    leftControl.onClicked: resourcesScreen.splash.open()
                    leftControl.enabled: camerasGrid.enabled
                }
            }
        ]
    }

    customRightControl: ToolBarButton
    {
        icon.source: "image://skin/24x24/Outline/more.svg?primary=light4"
        visible: resourcesScreen.contentItem === videoScreenLoader.item
        onClicked:
        {
            videoScreenLoader.item.menu.open()
        }
    }

    splashTitle: qsTr("Resources")
    splashItem: resourceTreeSheet

    leftPanel
    {
        title: qsTr("Resources")
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/resource_tree.svg?primary=dark1"
        interactive: true
        visible: true
        item: resourceTreeSheet
    }

    rightPanel
    {
        title: qsTr("Timeline")
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/timeline.svg?primary=dark1"
        interactive: true
        item: resourcesScreen.contentItem === videoScreenLoader.item
            ? videoScreenLoader.item.navigatorItem
            : null
    }

    ResourceTreeItem
    {
        id: resourceTreeSheet

        onLayoutSelected: (layoutResource) =>
        {
            resourcesScreen.filterIds = []
            videoScreenLoader.item?.controller.stop()
            windowContext.deprecatedUiController.resource = layoutResource
            resourcesScreen.contentItem = camerasGrid

            if (!LayoutController.isTabletLayout)
                splash.close()
        }

        onCameraSelected: (cameraResource) =>
        {
            if (!LayoutController.isTabletLayout)
                splash.close()

            windowContext.deprecatedUiController.resource = cameraResource
            if (LayoutController.isMobile)
            {
                Workflow.openVideoScreen(cameraResource)
            }
            else
            {
                resourcesScreen.filterIds = []

                resourcesScreen.contentItem = videoScreenLoader.item
                videoScreenLoader.item.controller.start(cameraResource, -1)
            }
        }

        onVisibleChanged:
        {
            if (!visible)
                cancelSearch()
        }
    }

    CamerasGrid
    {
        id: camerasGrid

        objectName: "camerasGrid"
        enabled: !windowContext.sessionManager.hasReconnectingSession && !loadingDummy.visible
        layout: resourceHelper.isLayout ? windowContext.deprecatedUiController.resource : null
        keepStatuses: !windowContext.sessionManager.hasReconnectingSession && !windowContext.sessionManager.hasConnectedSession
        active: resourcesScreen.isActive
        bottomMargin : LayoutController.isTabletLayout ? 20 : 0
        leftMargin : LayoutController.isTabletLayout ? 20 : 0
        rightMargin : LayoutController.isTabletLayout ? 20 : 0
        topMargin : LayoutController.isTabletLayout ? 20 : 0

        onOpenVideoScreen: (resource, thumbnailUrl, camerasModel) =>
        {
            stopMediaPlayers()

            if (LayoutController.isMobile)
            {
                Workflow.openVideoScreen(resource, thumbnailUrl, undefined, camerasModel)
            }
            else
            {
                videoScreenLoader.item.initialScreenshot = thumbnailUrl ?? ""
                videoScreenLoader.item.camerasModel = camerasModel
                videoScreenLoader.item.controller.start(resource, -1)

                resourcesScreen.filterIds = []
                resourcesScreen.contentItem = videoScreenLoader.item
            }
        }

        DummyMessage
        {
            id: noCamerasPlaceholder

            anchors.fill: parent
            title: qsTr("No Devices")
            titleColor: ColorTheme.colors.light4
            description:
            {
                return windowContext.deprecatedUiController.resource
                    ? qsTr("No devices were found on this layout")
                    : qsTr("No devices were found on this site. " +
                        "Add devices with the desktop client, " +
                        "or request access to existing devices")
            }
            descriptionColor: ColorTheme.colors.light10
            descriptionFontPixelSize: 15
            image: "image://skin/64x64/Outline/camera.svg?primary=light10"
            visible: camerasGrid.count === 0
                && windowContext.sessionManager.hasActiveSession
        }
    }

    Loader
    {
        id: videoScreenLoader

        active: LayoutController.isTablet

        sourceComponent: Component
        {
            VideoScreen
            {
                toolBar.visible: false
                backgroundColor: ColorTheme.colors.dark4
                ownsNavigator: !LayoutController.isTabletLayout
                padding: resourcesScreen.fullscreen
                    ? 0
                    : (LayoutController.isTabletLayout ? 20 : 0)

                Component.onCompleted:
                {
                    selectedObjectsType = appContext.settings.selectedObjectsType
                        ?? Timeline.ObjectsLoader.ObjectsType.motion
                }
            }
        }
    }

    ResourceHelper
    {
        id: resourceHelper
        resource: windowContext.deprecatedUiController.resource

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
        value:
        {
            let title = ""
            if (resourcesScreen.contentItem === camerasGrid)
                title = camerasGrid.layout?.name ?? qsTr("All Devices")
            else if (resourcesScreen.contentItem === videoScreenLoader.item)
                title = videoScreenLoader.item.title

            return title || windowContext.sessionManager.systemName
        }

        when: windowContext.sessionManager.hasActiveSession
    }

    Item
    {
        id: overlayItem

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

            Button
            {
                id: stopConnectingButton

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.bottomMargin: 24

                text: qsTr("Stop Connecting")
                opacity: 0
                onClicked: windowContext.sessionManager.stopSessionByUser()

                SequentialAnimation
                {
                    id: showAnimation

                    running: windowContext.sessionManager.hasConnectingSession
                        || windowContext.sessionManager.hasAwaitingResourcesSession

                    PauseAnimation { duration: 2000 }

                    OpacityAnimator
                    {
                        target: stopConnectingButton
                        from: 0
                        to: 1
                        duration: 200
                    }
                }
            }
        }
    }

    customBackHandler: (isEscKeyPressed) =>
    {
        if (loadingDummy.visible)
            windowContext.sessionManager.stopSessionByUser()
        else if (!isEscKeyPressed)
            mainWindow.close()
    }

    Component.onCompleted:
    {
        if (contentItem === videoScreenLoader.item)
            videoScreenLoader.item.controller.start(resourceHelper.resource, -1)
    }
}

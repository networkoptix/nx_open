// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Controls
import Nx.Common
import Nx.Core
import Nx.Core.Controls
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Mobile.Ui.Sheets
import Nx.Screens
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile
import nx.vms.client.mobile.timeline as Timeline

import "private"
import "private/ResourcesScreen"

AdaptiveScreen
{
    id: resourcesScreen

    objectName: "resourcesScreen"

    property alias filterIds: camerasGrid.filterIds

    function closeVideoScreen()
    {
        LayoutController.exitFullscreen()

        videoScreenLoader.item?.controller.stop()

        if (!resourceHelper.isLayout)
            windowContext.deprecatedUiController.resource = null

        resourcesScreen.showsVideoScreen = false
    }

    // Persist `false` whenever AdaptiveScreen closes a panel (close button, auto-close, or
    // cross-close). The matching "open → persist `true`" is handled by each panel's
    // `onVisibleChanged` below.
    onPanelClosed: (panel) =>
    {
        if (panel === leftPanel)
            appContext.settings.resourcesPanelVisible = false
        else if (panel === rightPanel)
            appContext.settings.timelinePanelVisible = false
    }

    titleUnderlineVisible: siteToolTip.available

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

    toolBar.titleControl
    {
        leftPadding: recordingStatusIndicator.sourcePath ? recordingStatusIndicator.width : 0
        horizontalAlignment: Qt.AlignLeft
    }

    RecordingStatusIndicator
    {
        id: recordingStatusIndicator

        parent: toolBar.titleControl
        anchors.verticalCenter: parent.verticalCenter

        resource: resourcesScreen.contentItem === videoScreenLoader.item
            ? videoScreenLoader.item.controller.resource
            : null
    }

    // Whether the content area shows the embedded video screen instead of the cameras grid. The
    // video screen is embedded in every layout, only the timeline placement differs: a side panel
    // in the two-pane shell, stacked under the video otherwise.
    property bool showsVideoScreen: false

    // Kept a binding rather than assigned imperatively: an item assigned to `contentItem` would
    // become null if the loader item ever went away, leaving an empty content area behind and
    // making every `contentItem === videoScreenLoader.item` check below true.
    contentItem: showsVideoScreen && videoScreenLoader.item
        ? videoScreenLoader.item
        : camerasGrid
    overlayItem: overlayItem
    longContent: contentItem === videoScreenLoader.item

    // The video screen has its own controls at the bottom of the content area, so the panel
    // buttons go in line with them there. Over the cameras grid they keep floating in the corners.
    // Only the layouts which have the side panels dock the buttons: elsewhere the video screen
    // must not reserve the room for them in its navigation bar.
    leftPanelButtonContainer: LayoutController.hasSidePanels
            && contentItem === videoScreenLoader.item
        ? videoScreenLoader.item.leftPanelButtonSlot
        : null
    rightPanelButtonContainer: LayoutController.hasSidePanels
            && contentItem === videoScreenLoader.item
        ? videoScreenLoader.item.rightPanelButtonSlot
        : null

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
                        || (resourceHelper.isCamera && !LayoutController.hasSidePanels))

                PropertyChanges
                {
                    leftControl.icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=%1"
                        .arg(StyleHints.foregroundColorName)
                    leftControl.onClicked: resourcesScreen.closeVideoScreen()
                }
            },
            State
            {
                name: "openResourceTreeSplash"
                when: !LayoutController.hasSidePanels

                PropertyChanges
                {
                    leftControl.icon.source:
                        "image://skin/24x24/Outline/resource_tree.svg?primary=%1"
                            .arg(StyleHints.foregroundColorName)
                    leftControl.onClicked: resourcesScreen.splash.open()
                    leftControl.enabled: camerasGrid.enabled
                }
            }
        ]
    }

    // The embedded video screen keeps its own tool bar hidden, so its kebab is the one of this
    // screen.
    menuButton
    {
        visible: resourcesScreen.contentItem === videoScreenLoader.item

        onClicked:
            videoScreenLoader.item.menu.open()
    }

    splashTitle: qsTr("Resources")
    splashItem: resourceTreeSheet

    leftPanel
    {
        title: qsTr("Resources")
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/resource_tree.svg?primary=dark1"
        interactive: true
        item: resourceTreeSheet

        onVisibleChanged:
        {
            if (visible && leftPanel.item)
                appContext.settings.resourcesPanelVisible = true
        }
    }

    Binding
    {
        id: initialLeftPanelVisibilityBinding

        when: leftPanel.item
        target: leftPanel
        property: "visible"
        value: appContext.settings.resourcesPanelVisible
        restoreMode: Binding.RestoreNone
    }

    rightPanel
    {
        title:
        {
            let displayedDataType = rightPanel.item?.displayedDataType ?? ""
            return displayedDataType || qsTr("Timeline")
        }
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/timeline.svg?primary=dark1"
        interactive: true

        // The timeline goes into this panel only where there is one. Complementary to
        // `ownsNavigator` below, which lets the video screen keep the timeline under the video.
        item: LayoutController.hasSidePanels
                && resourcesScreen.contentItem === videoScreenLoader.item
            ? videoScreenLoader.item.navigatorItem
            : null

        onVisibleChanged:
        {
            if (visible && rightPanel.item)
                appContext.settings.timelinePanelVisible = true
        }
    }

    Binding
    {
        id: initialRightPanelVisibilityBinding

        when: rightPanel.item
        target: rightPanel
        property: "visible"
        value: appContext.settings.timelinePanelVisible
        restoreMode: Binding.RestoreNone
    }

    Connections
    {
        target: LayoutController

        function onFullscreenChanged()
        {
            if (!resourcesScreen.isActive)
                return

            if (LayoutController.fullscreen)
                return

            if (leftPanel.item)
                leftPanel.visible = appContext.settings.resourcesPanelVisible
            if (rightPanel.item)
                rightPanel.visible = appContext.settings.timelinePanelVisible
        }
    }

    ResourceTreeItem
    {
        id: resourceTreeSheet

        onLayoutSelected: (layoutResource) =>
        {
            resourcesScreen.filterIds = []
            videoScreenLoader.item?.controller.stop()
            windowContext.deprecatedUiController.resource = layoutResource
            resourcesScreen.showsVideoScreen = false

            if (!LayoutController.hasSidePanels)
                splash.close()
        }

        onCameraSelected: (cameraResource) =>
        {
            if (!LayoutController.hasSidePanels)
                splash.close()

            camerasGrid.stopMediaPlayers()

            windowContext.deprecatedUiController.resource = cameraResource

            // Filter out all the cameras except selected to prevent ability to swipe between
            // cameras.
            videoScreenLoader.item.defaultCamerasModel.filterIds = [cameraResource.id]
            videoScreenLoader.item.camerasModel = videoScreenLoader.item.defaultCamerasModel
            videoScreenLoader.item.controller.start(cameraResource, -1)

            resourcesScreen.showsVideoScreen = true
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
        active: resourcesScreen.isActive && !resourcesScreen.showsVideoScreen
        bottomMargin : LayoutController.hasSidePanels ? 20 : 0
        leftMargin : LayoutController.hasSidePanels ? 20 : 0
        rightMargin : LayoutController.hasSidePanels ? 20 : 0
        topMargin : LayoutController.hasSidePanels ? 20 : 0

        onOpenVideoScreen: (resource, thumbnailUrl, camerasModel) =>
        {
            stopMediaPlayers()

            videoScreenLoader.item.camerasModel = camerasModel
            videoScreenLoader.item.controller.start(resource, -1)

            resourcesScreen.showsVideoScreen = true
        }

        DummyMessage
        {
            id: noCamerasPlaceholder

            anchors.centerIn: parent
            width: Math.min(parent.width, 247)
            title: qsTr("No Devices")
            description:
            {
                return windowContext.deprecatedUiController.resource
                    ? qsTr("No devices were found on this layout")
                    : qsTr("No devices were found on this site. " +
                        "Add devices with the desktop client, " +
                        "or request access to existing devices")
            }
            image: "image://skin/64x64/Outline/nodevices.svg?primary=light10"
            visible: camerasGrid.count === 0
                && windowContext.sessionManager.hasActiveSession
        }
    }

    Loader
    {
        id: videoScreenLoader

        clip: true

        sourceComponent: Component
        {
            VideoScreen
            {
                id: modernVideoScreen //< For the FT purposes.
                activePage: resourcesScreen.isActive && resourcesScreen.showsVideoScreen

                toolBar.visible: false
                backgroundColor: ColorTheme.colors.dark4
                ownsNavigator: !LayoutController.hasSidePanels

                leftPanelButtonWanted: resourcesScreen.leftPanelButtonWanted
                rightPanelButtonWanted: resourcesScreen.rightPanelButtonWanted

                // The visible kebab button lives in resourcesScreen's toolBar (above), so
                // anchor the menu to it instead of VideoScreen's hidden internal kebab.
                menuAnchor: resourcesScreen.menuButton

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
                title = videoScreenLoader.item?.title

            return title || windowContext.sessionManager.systemName
        }

        when: windowContext.sessionManager.hasActiveSession
    }

    Item
    {
        id: overlayItem

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

    SiteToolTip
    {
        id: siteToolTip

        readonly property var controller: resourcesScreen.contentItem === videoScreenLoader.item
            ? videoScreenLoader.item.controller
            : null

        toolBar: resourcesScreen.toolBar
        text: controller?.resourceHelper.crossSystemName ?? ""
    }

    customBackHandler: (isEscKeyPressed) =>
    {
        if (resourcesScreen.showsVideoScreen)
        {
            resourcesScreen.closeVideoScreen()
            return
        }

        if (loadingDummy.visible)
            windowContext.sessionManager.stopSessionByUser()
        else if (!isEscKeyPressed)
            mainWindow.close()
    }

    Component.onCompleted:
    {
        if (!resourceHelper.isCamera)
            return

        showsVideoScreen = true
        videoScreenLoader.item.controller.start(resourceHelper.resource, -1)
    }
}

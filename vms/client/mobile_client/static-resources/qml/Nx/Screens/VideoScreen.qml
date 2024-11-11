// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Common
import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Mobile
import Nx.Models
import Nx.Ui

import nx.vms.client.core

import "private/VideoScreen"
import "private/VideoScreen/utils.js" as VideoScreenUtils
import "private/VideoScreen/Ptz"

Page
{
    id: videoScreen
    objectName: "videoScreen"

    property Resource initialResource
    property alias initialScreenshot: screenshot.source

    property QnCameraListModel camerasModel:
        QnCameraListModel {} //< Keeps ability to switch between cameras when screen is opened by link.

    property real targetTimestamp: -1

    backgroundColor: "black"

    clip: false

    VideoScreenController
    {
        id: controller

        navigator: navigator

        mediaPlayer.videoQuality: settings.lastUsedQuality

        mediaPlayer.onPlayingChanged:
        {
            if (mediaPlayer.playing)
                screenshot.source = ""
        }

        onOfflineChanged:
        {
            if (offline)
            {
                offlineStatusDelay.restart()
                ptzPanel.moveOnTapMode = false
            }
            else
            {
                offlineStatusDelay.stop()
                d.showOfflineStatus = false
            }
        }

        resourceHelper.onResourceRemoved: Workflow.popCurrentScreen()
    }

    NxObject
    {
        id: d

        property bool animatePlaybackControls: true
        property bool showOfflineStatus: false
        property bool showNoLicensesWarning:
            controller.noLicenses
            && !controller.mediaPlayer.liveMode
        property bool showDefaultPasswordWarning:
            controller.hasDefaultCameraPassword
            && controller.mediaPlayer.liveMode

        // TODO: Rely only on controller.dummyState == ""
        property bool cameraWarningVisible:
            ((showOfflineStatus
                || controller.cameraOffline
                || controller.cameraUnauthorized
                || controller.failed
                || controller.noVideoStreams)
                && !controller.mediaPlayer.playing)
            || controller.hasOldFirmware
            || controller.tooManyConnections
            || controller.cannotDecryptMedia
            || controller.ioModuleWarning
            || controller.ioModuleAudioPlaying
            || controller.liveVirtualCamera
            || controller.audioOnlyMode
            || showDefaultPasswordWarning
            || showNoLicensesWarning

        readonly property bool applicationActive: Qt.application.state === Qt.ApplicationActive

        property bool uiVisible: true
        property real uiOpacity: uiVisible ? 1.0 : 0.0

        Behavior on uiOpacity
        {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }

        property bool controlsVisible: true
        property real controlsOpacity: controlsVisible ? 1.0 : 0.0
        Behavior on controlsOpacity
        {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }

        property real cameraUiOpacity: 1.0

        property int mode: VideoScreenUtils.VideoScreenMode.Navigation
        readonly property bool ptzMode: mode === VideoScreenUtils.VideoScreenMode.Ptz
        onPtzModeChanged: navigator.motionSearchMode = false

        Timer
        {
            id: offlineStatusDelay
            interval: 2000
            onTriggered: d.showOfflineStatus = true
        }

        onCameraWarningVisibleChanged:
        {
            if (cameraWarningVisible)
            {
                if (controller.serverOffline)
                {
                    exitFullscreen()
                    controlsVisible = false
                    uiVisible = true
                }
                else if (controller.cameraOffline)
                {
                    showUi()
                }
            }
            else
            {
                d.controlsVisible = true
            }
        }

        onApplicationActiveChanged:
        {
            if (!applicationActive && !ptzPanel.moveOnTapMode)
                showUi()
        }
    }

    sideNavigationEnabled: false
    onLeftButtonClicked: Workflow.popCurrentScreen()

    title: controller.resourceHelper.resourceName
    toolBar.opacity: d.uiOpacity
    toolBar.visible: opacity > 0
    titleLabelOpacity: d.cameraUiOpacity
    titleControls:
        [
            MotionAreaButton
            {
                visible: video.motionController.customRoiExists
                anchors.verticalCenter: parent.verticalCenter

                text: qsTr("Area")
                icon.source: lp("/images/close.png")

                onClicked: video.motionController.clearCustomRoi()
            },

            IconButton
            {
                id: menuButton

                icon.source: lp("/images/more_vert.png")
                onClicked:
                {
                    menu.open()
                }
            }
        ]

    toolBar.contentItem.clip: false
    gradientToolbarBackground: true

    Banner
    {
        id: banner

        property bool portraitOrientation:
            Screen.orientation === Qt.PortraitOrientation
            || Screen.orientation === Qt.InvertedPortraitOrientation

        parent: toolBar
        y: portraitOrientation ? parent.height : 8
        x: (parent.width - width) / 2
        maxWidth: portraitOrientation ? parent.width - 2 * 16 : 360
    }

    Menu
    {
        id: menu

        parent: menuButton

        MenuItem
        {
            text: qsTr("Camera Settings")
            onClicked:
            {
                Workflow.openCameraSettingsScreen(
                    controller.mediaPlayer,
                    controller.resourceHelper.audioSupported,
                    controller.audioController)
            }
        }

        MenuItem
        {
            id: bookmarksMenuItem
            text: qsTr("Bookmarks")
            height: visible ? implicitHeight : 0
            onTriggered: Workflow.openEventSearchScreen(controller.resource.id, camerasModel)
        }

        MenuItem
        {
            id: objectsMenuItem

            text: qsTr("Objects")
            height: visible ? implicitHeight : 0
            onTriggered: Workflow.openEventSearchScreen(controller.resource.id, camerasModel, true)
        }

        onAboutToShow:
        {
            bookmarksMenuItem.visible = hasViewBookmarksPermission()
            objectsMenuItem.visible = hasSearchObjectsPermission()
        }
    }

    MouseArea
    {
        id: toggleMouseArea

        anchors.fill: parent
        onClicked:
        {
            if (!ptzPanel.moveOnTapMode)
                toggleUi()
        }
    }

    ScalableVideo
    {
        id: video

        y: toolBar.visible ? -header.height : 0
        x: -mainWindow.leftPadding
        width: mainWindow.width
        height: mainWindow.height

        visible: dummyLoader.status != Loader.Ready && !screenshot.visible
        opacity: d.cameraUiOpacity

        resourceHelper: controller.resourceHelper
        mediaPlayer: controller.mediaPlayer
        videoCenterHeightOffsetFactor: 1 / 3
        motionController.motionSearchMode: navigator.motionSearchMode
        motionController.enabled: navigator.hasArchive && !d.ptzMode

        onClicked: toggleUi()

        readonly property string roiHintText: qsTr("Tap and hold to select an area")
        onHideRoiHint:
        {
            if (banner.text == roiHintText)
                banner.hide()
        }

        onShowRoiHint: banner.showText(roiHintText)

        Connections
        {
            target: video.motionController

            function onDrawingRoiChanged()
            {
                if (!target.drawingRoi)
                    return

                navigator.motionSearchMode = true
                showUi()
            }

            function onRequestUnallowedDrawing()
            {
                banner.showText(qsTr("Enable motion search first to select an area"))
            }

            function onEmptyRoiCleared()
            {
                banner.showText(qsTr("Invalid custom area. Please draw a correct one."))
            }
        }
    }

    Item
    {
        id: screenshotPlaceholder

        anchors.fill: video

        Image
        {
            id: screenshot

            function getAspect(value)
            {
                return value.width > 0 && value.height > 0 ? value.width / value.height : 1
            }

            function fitToBounds(value, bounds)
            {
                var aspect = getAspect(value)
                return aspect < bounds.width / bounds.height
                    ? Qt.size(bounds.height * aspect, bounds.height)
                    : Qt.size(bounds.width, bounds.width / aspect)
            }

            function fillBounds(value, bounds)
            {
                var aspect = getAspect(value)
                var minimalSize = Math.min(bounds.width, bounds.height)
                return value.width < value.height
                    ? Qt.size(minimalSize, minimalSize / aspect)
                    : Qt.size(minimalSize * aspect, minimalSize)
            }

            readonly property size boundingSize:
            {
                var windowSize = Qt.size(parent.width, parent.height)
                return video.fisheyeMode
                    ? fillBounds(sourceSize, windowSize)
                    : fitToBounds(sourceSize, windowSize)
            }

            width: boundingSize.width
            height: boundingSize.height

            y: (mainWindow.height - height) / 3
            x: (mainWindow.width - width) / 2 - mainWindow.leftPadding
            visible: false//status == Image.Ready && !dummyLoader.visible && source != ""
            opacity: d.cameraUiOpacity
        }
    }

    Image
    {
        id: controlsShadowGradient

        property real customHeight:
        {
            if (d.ptzMode || ptzPanel.moveOnTapMode)
                return getNavigationBarSize()

            return navigator.buttonsPanelHeight + getNavigationBarSize()
        }

        x: -mainWindow.leftPadding
        width: mainWindow.width
        height: 96 + customHeight
        anchors.bottom: parent.bottom
        anchors.bottomMargin: videoScreen.height - mainWindow.height

        visible: opacity > 0
        source: lp("/images/timeline_gradient.png")

        opacity:
        {
            var visiblePtzControls = d.ptzMode && d.uiVisible
            if (visiblePtzControls || ptzPanel.moveOnTapMode)
                return 1

            return d.ptzMode ? d.uiOpacity : navigator.opacity
        }
    }

    Item
    {
        id: content

        width: mainWindow.availableWidth
        height: mainWindow.availableHeight - header.height
        y: header.visible ? 0 : header.height

        Loader
        {
            id: informationLabelLoader
            anchors.right: parent.right
            anchors.rightMargin: 8
            opacity: Math.min(d.uiOpacity, d.cameraUiOpacity)
            active: showCameraInfo
            sourceComponent: InformationLabel
            {
                videoScreenController: controller
            }
        }

        Loader
        {
            id: dummyLoader

            readonly property bool needOffset: item && item.onlyCompactTitleIsVisible

            y: needOffset ? -header.height : 0
            x: -mainWindow.leftPadding
            width: mainWindow.width
            height: mainWindow.height - (needOffset ? 0 : header.height)

            visible: active
            active: d.cameraWarningVisible

            sourceComponent: Component
            {
                VideoDummy
                {
                    readonly property bool onlyCompactTitleIsVisible:
                        compact && title != "" && description == "" && buttonText == ""

                    rightPadding: 8 + mainWindow.rightPadding
                    leftPadding: 8 + mainWindow.leftPadding
                    compact: videoScreen.height < 540
                    state: controller.dummyState

                    MouseArea
                    {
                        anchors.fill: parent
                        onClicked: toggleUi()
                    }
                }
            }
        }

        PtzPanel
        {
            id: ptzPanel

            preloaders.parent: video
            preloaders.height: video.fitSize ? video.fitSize.height : 0
            preloaders.x: (video.width - preloaders.width) / 2
            preloaders.y: (video.height - preloaders.height) / 3

            width: parent.width
            anchors.bottom: parent.bottom

            controller.resource: controller.resource
            customRotation: controller.resourceHelper.customRotation

            opacity: Math.min(d.uiOpacity, d.controlsOpacity)
            visible: opacity > 0 && d.ptzMode

            onCloseButtonClicked: d.mode = VideoScreenUtils.VideoScreenMode.Navigation

            onMoveOnTapModeChanged:
            {
                if (ptzPanel.moveOnTapMode)
                {
                    hideUi()
                    moveOnTapOverlay.open()
                    video.fitToBounds()
                }
                else
                {
                    showUi()
                    moveOnTapOverlay.close()
                }
            }
        }

        PtzViewportMovePreloader
        {
            id: preloader

            parent: videoScreen
            visible: false
        }

        MoveOnTapOverlay
        {
            id: moveOnTapOverlay

            x: -mainWindow.leftPadding
            width: mainWindow.width
            height: mainWindow.height
            parent: videoScreen

            onClicked:
            {
                if (controller.resourceHelper.fisheyeParams.enabled || !video)
                    return

                var mapped = contentItem.mapToItem(video, pos.x, pos.y)
                var data = video.getMoveViewportData(mapped)
                if (!data)
                    return

                ptzPanel.moveViewport(data.viewport, data.aspect)
                preloader.pos = contentItem.mapToItem(preloader.parent, pos.x, pos.y)
                preloader.visible = true
            }

            onVisibleChanged:
            {
                if (moveOnTapOverlay.visible)
                    return

                showUi()
                ptzPanel.moveOnTapMode = false
            }
        }

        VideoNavigation
        {
            id: navigator

            drawingRoi: video.motionController.motionSearchMode && video.motionController.drawingRoi
            changingMotionRoi:
            {
                return video.motionController.drawingRoi
                    ? false
                    : video.moving && !video.motionController.customRoiExists
            }

            onWarningTextChanged: banner.showText(warningText)

            hasCustomRoi: video.motionController.customRoiExists
            hasCustomVisualArea: video.motionController.hasCustomVisualArea
            canViewArchive: controller.accessRightsHelper.canViewArchive
            animatePlaybackControls: d.animatePlaybackControls
            controller: controller
            controlsOpacity: d.cameraUiOpacity
            onPtzButtonClicked:
            {
                d.mode = VideoScreenUtils.VideoScreenMode.Ptz
                video.to1xScale()
            }

            anchors.bottom: parent.bottom
            width: parent.width

            visible: opacity > 0 && d.mode === VideoScreenUtils.VideoScreenMode.Navigation
            opacity: Math.min(d.uiOpacity, d.controlsOpacity)
            onSwitchToPreviousCamera:
            {
                if (!camerasModel)
                    return

                switchToCamera(camerasModel.previousResource(controller.resource))
            }

            onSwitchToNextCamera:
            {
                if (!camerasModel)
                    return

                switchToCamera(camerasModel.nextResource(controller.resource))
            }

            motionFilter: video.fisheyeMode ? "" : video.motionController.motionFilter
        }
    }

    Rectangle
    {
        id: bottomControlsBackground

        color: videoScreen.backgroundColor
        width: mainWindow.width
        height: mainWindow.height - videoScreen.height
        anchors.top: content.bottom
        z: -1
    }

    Rectangle
    {
        id: navigationBarTint

        color: ColorTheme.colors.dark3
        visible: mainWindow.hasNavigationBar
        width: mainWindow.width - parent.width
        height: video.height
        x: mainWindow.leftPadding ? -mainWindow.leftPadding : parent.width
        anchors.top: video.top
        opacity: Math.min(navigator.opacity, d.cameraUiOpacity)
    }

    SequentialAnimation
    {
        id: cameraSwitchAnimation

        property Resource newResource
        property string thumbnail

        NumberAnimation
        {
            target: d
            property: "cameraUiOpacity"
            to: 0.0
            duration: 200
        }

        ScriptAction
        {
            script:
            {
                d.animatePlaybackControls = false
                controller.setResource(cameraSwitchAnimation.newResource)
                initialScreenshot = cameraSwitchAnimation.thumbnail
                d.animatePlaybackControls = true
            }
        }

        NumberAnimation
        {
            target: d
            property: "cameraUiOpacity"
            to: 1.0
            duration: 200
        }
    }

    ModelDataAccessor
    {
        id: camerasModelAccessor
        model: camerasModel
    }

    onActivePageChanged:
    {
        if (activePage && initialResource)
        {
            controller.start(initialResource, targetTimestamp)
            initialResource = null
            targetTimestamp = -1
        }
    }

    Component.onDestruction: exitFullscreen()

    function hideUi()
    {
        d.uiVisible = false
        if (CoreUtils.isMobile())
            enterFullscreen()
    }

    function showUi()
    {
        exitFullscreen()
        d.uiVisible = true
    }

    function toggleUi()
    {
        if (d.uiVisible)
            hideUi()
        else
            showUi()
    }

    function switchToCamera(resource)
    {
        navigator.motionSearchMode = false
        cameraSwitchAnimation.stop()
        cameraSwitchAnimation.newResource = resource
        if (controller.mediaPlayer.liveMode)
        {
            cameraSwitchAnimation.thumbnail = camerasModelAccessor.getData(
                camerasModel.rowByResourceId(resource.id), "thumbnail")
        }
        else
        {
            cameraSwitchAnimation.thumbnail = ""
        }

        cameraSwitchAnimation.start()
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Nx.Common
import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Models
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile.timeline as Timeline
import nx.vms.common

import "private/VideoScreen"
import "private/VideoScreen/utils.js" as VideoScreenUtils
import "private/VideoScreen/Ptz"
import "private/VideoScreen/Timeline" as Timeline

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

    // TODO: FIXME! Control left handed / right handed layouts via settings.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    VideoScreenController
    {
        id: controller

        chunkContentType:
        {
            switch (timeline.objectsType)
            {
                case Timeline.ObjectsLoader.ObjectsType.motion:
                    return CommonGlobals.MotionContent
                case Timeline.ObjectsLoader.ObjectsType.analytics:
                    return CommonGlobals.AnalyticsContent
                default:
                    return CommonGlobals.RecordingContent
            }
        }

        forceVideoPause: d.forceVideoPause
        chunkProvider: cameraChunkProvider

        timelineController: Timeline.Controller
        {
            controller: controller
            timeline: timeline
        }

        mediaPlayer.videoQuality: appContext.settings.lastUsedQuality

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
            && !controller.playingLive
        property bool showDefaultPasswordWarning:
            controller.hasDefaultCameraPassword
            && controller.playingLive

        // TODO: Rely only on controller.dummyState == ""
        property bool cameraWarningVisible:
            ((showOfflineStatus
                || controller.cameraOffline
                || controller.cameraUnauthorized
                || controller.needsCloudAuthorization
                || controller.systemConnecting
            // TODO: #vkutin Add a wait for media loaded state to Mobile FTs to enable this.
            //  || controller.mediaLoading
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

        property bool forceVideoPause: false

        readonly property bool canViewArchive: controller.accessRightsHelper.canViewArchive
        readonly property bool hasArchive: canViewArchive && cameraChunkProvider.bottomBound >= 0

        readonly property bool applicationActive: Qt.application.state === Qt.ApplicationActive

        // TODO: Implement free text search mode.
        property bool searchActive: false
        property int searchResults: 0
        property int searchIndex: 0

        property bool controlsVisible: true
        property real controlsOpacity: controlsVisible ? 1.0 : 0.0
        Behavior on controlsOpacity
        {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }

        property real cameraUiOpacity: 1.0

        property int mode: VideoScreenUtils.VideoScreenMode.Navigation
        readonly property bool ptzMode: mode === VideoScreenUtils.VideoScreenMode.Ptz

        Timer
        {
            id: offlineStatusDelay
            interval: 2000
            onTriggered: d.showOfflineStatus = true
        }

        onCameraWarningVisibleChanged:
        {
            d.controlsVisible = !(d.cameraWarningVisible && controller.serverOffline)
        }
    }

    onLeftButtonClicked: Workflow.popCurrentScreen()

    rightControl: IconButton
    {
        id: menuButton

        anchors.centerIn: parent

        icon.source: lp("/images/more_vert.png")
        onClicked:
        {
            menu.x = 0
            menu.y = height
            menu.open()
        }
    }

    title: controller.resourceHelper.qualifiedResourceName
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
            visible: (controller.systemContext?.hasViewBookmarksPermission
                && !(controller.resource?.flags & ResourceFlag.cross_system)) ?? false
            height: visible ? implicitHeight : 0
            onTriggered: Workflow.openEventSearchScreen(controller.resource.id, camerasModel)
        }

        MenuItem
        {
            id: objectsMenuItem
            text: qsTr("Objects")
            visible: (controller.systemContext?.hasSearchObjectsPermission
                && !(controller.resource?.flags & ResourceFlag.cross_system)) ?? false
            height: visible ? implicitHeight : 0
            onTriggered: Workflow.openEventSearchScreen(controller.resource.id, camerasModel, true)
        }
    }

    ScalableVideo
    {
        id: video

        y: 0
        x: -windowParams.leftMargin
        width: mainWindow.width
        height: width / 1.6

        visible: dummyLoader.status != Loader.Ready && !screenshot.visible
        opacity: d.cameraUiOpacity

        resourceHelper: controller.resourceHelper
        mediaPlayer: controller.mediaPlayer
        videoCenterHeightOffsetFactor: 1 / 3
        motionController.motionSearchMode: controller.contentType === CommonGlobals.MotionContent
        motionController.enabled: d.hasArchive && !d.ptzMode

        readonly property string roiHintText: qsTr("Tap and hold to select an area")

        onHideRoiHint:
        {
            if (banner.text == roiHintText)
                banner.hide()
        }

        onShowRoiHint:
            banner.showText(roiHintText)

        Connections
        {
            target: video.motionController

            function onDrawingRoiChanged()
            {
                if (target.drawingRoi)
                    objectsTypeSheet.selectedType = Timeline.ObjectsLoader.ObjectsType.motion
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

    NxDotPreloader
    {
        anchors.centerIn: video
        color: ColorTheme.colors.light16
        dotRadius: 6
        spacing: 8

        running: controller.mediaPlayer.loading
            && controller.mediaPlayer.playbackState !== MediaPlayer.Previewing
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
            x: (mainWindow.width - width) / 2 - windowParams.leftMargin
            visible: false//status == Image.Ready && !dummyLoader.visible && source != ""
            opacity: d.cameraUiOpacity
        }
    }

    component ControlButton: Button
    {
        property bool suppressDisabledState: false

        readonly property string effectiveState: suppressDisabledState && !enabled
            ? (checked ? "default_checked" : "default_unchecked")
            : state

        backgroundColor: parameters.colors[effectiveState]
        foregroundColor: parameters.textColors[effectiveState]

        leftPadding: 10
        rightPadding: 10
        icon.width: 24
        icon.height: 24
    }

    Rectangle
    {
        id: navigationBar

        width: mainWindow.width
        height: navigationBarContent.height + navigationBarContent.contentMargin * 2
        anchors.top: video.bottom

        color: ColorTheme.colors.dark4

        readonly property bool hasChunkNavigation:
            objectsTypeSheet.selectedType !== Timeline.ObjectsLoader.ObjectsType.bookmarks

        Item
        {
            id: navigationBarContent

            readonly property real contentMargin: 12

            component ExpandCollapseAnimation: NumberAnimation { duration: 150 }

            anchors.left: navigationBar.left
            anchors.leftMargin: contentMargin
            anchors.right: navigationBar.right
            anchors.rightMargin: contentMargin

            anchors.verticalCenter: navigationBar.verticalCenter
            height: navigationBarLayout.height

            RowLayout
            {
                id: navigationBarLayout

                width: Math.max(implicitWidth, navigationBarContent.width)
                anchors.horizontalCenter: navigationBarContent.horizontalCenter

                spacing: 0

                ControlButton
                {
                    id: calendarButton

                    icon.source: "image://skin/24x24/Solid/calendar.svg"
                    enabled: d.hasArchive

                    onClicked:
                        calendarPanel.open()
                }

                Item
                {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Row
                {
                    leftPadding: 8
                    rightPadding: 8

                    spacing: speedControl.expanded ? navigationBarContent.contentMargin : 8
                    Behavior on spacing { ExpandCollapseAnimation {}}

                    LayoutMirroring.enabled: false

                    ControlButton
                    {
                        id: prevChunkButton

                        icon.source: "image://skin/24x24/Outline/chunk_previous.svg"
                        enabled: d.hasArchive && navigationBar.hasChunkNavigation

                        onClicked:
                            controller.jumpBackward()
                    }

                    ButtonBar
                    {
                        id: playBar

                        enabled: d.hasArchive
                        opacity: enabled ? 1.0 : 0.3
                        layer.enabled: opacity < 1.0

                        ControlButton
                        {
                            id: playPauseButton

                            suppressDisabledState: !playBar.enabled
                            width: 60

                            icon.source: controller.playing
                                ? "image://skin/24x24/Outline/pause.svg"
                                : "image://skin/24x24/Outline/play_small.svg"

                            onClicked:
                            {
                                if (controller.playing)
                                    controller.pause()
                                else
                                    controller.play()
                            }
                        }

                        Rectangle
                        {
                            id: speedControl

                            readonly property real speed: speedSlider.value < 0
                                ? -Math.pow(2.0, -speedSlider.value - 1.0)
                                : Math.pow(2.0, speedSlider.value)

                            property bool expanded: false

                            readonly property real expandedWidth: navigationBarContent.width
                                - playPauseButton.width - playBar.spacing

                            color: playBar.backgroundColor
                            radius: playBar.roundingRadius
                            height: speedButton.height
                            clip: true

                            width: expanded ? expandedWidth : speedButton.width
                            Behavior on width { ExpandCollapseAnimation {}}

                            ControlButton
                            {
                                id: speedButton

                                enabled: !speedControl.expanded
                                suppressDisabledState: !playBar.enabled || speedControl.expanded
                                width: 60

                                text: `${speedControl.speed}x`

                                onClicked:
                                    speedControl.expanded = true
                            }

                            RowLayout
                            {
                                anchors.left: speedButton.right
                                spacing: 0

                                height: speedControl.height
                                width: speedControl.expandedWidth - speedButton.width

                                opacity: speedControl.expanded ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 250 }}

                                visible: opacity > 0.0

                                Slider
                                {
                                    id: speedSlider

                                    enabled: !controller.playingLive
                                    opacity: (playBar.enabled && !enabled) ? 0.3 : 1.0

                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.fillWidth: true

                                    from: 0 //< TODO: Fix reverse in the player and make this "-5".
                                    to: 4
                                    stepSize: 1
                                    snapMode: Slider.SnapOnRelease
                                }

                                ControlButton
                                {
                                    id: speedControlCollapseButton

                                    leftPadding: 18
                                    rightPadding: 18
                                    icon.source: "image://skin/24x24/Solid/cancel.svg"
                                    Layout.alignment: Qt.AlignVCenter

                                    onClicked:
                                        speedControl.expanded = false
                                }
                            }

                            Binding
                            {
                                controller.speed: speedControl.speed
                            }
                        }

                        onEnabledChanged:
                        {
                            if (!enabled)
                                speedControl.expanded = false
                        }

                        Connections
                        {
                            target: controller

                            function onPlayingLiveChanged()
                            {
                                if (controller.playingLive)
                                    speedSlider.value = 0
                            }
                        }
                    }

                    ControlButton
                    {
                        id: nextChunkButton

                        icon.source: "image://skin/24x24/Outline/chunk_future.svg"

                        enabled: d.hasArchive && navigationBar.hasChunkNavigation
                            && !controller.playingLive

                        onClicked:
                            controller.jumpForward()
                    }
                }

                Item
                {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                ControlButton
                {
                    id: actionsButton

                    icon.source: "image://skin/24x24/Outline/grid_view.svg"
                    enabled: controller.playingLive
                }
            }
        }
    }

    Item
    {
        id: navigator

        anchors.top: navigationBar.bottom
        anchors.bottom: parent.bottom
        width: mainWindow.width

        visible: d.canViewArchive

        DataTimeline
        {
            id: timeline

            z: 1
            width: navigator.width
            anchors.top: navigator.top
            anchors.bottom: bottomBar.top

            chunkProvider: cameraChunkProvider
            objectsType: objectsTypeSheet.selectedType

            interactive: !objectsTypeSheet.opened && !timelineObjectSheet.opened

            onObjectTileTapped: (data) =>
            {
                if (!data?.perObjectData)
                    return

                timelineObjectSheet.fallbackImagePaths = data.imagePaths ?? []
                timelineObjectSheet.model = data.perObjectData
                timelineObjectSheet.open()
            }
        }

        Timeline.ObjectDetailsSheet
        {
            id: timelineObjectSheet
        }

        Rectangle
        {
            id: bottomBar

            width: navigator.width
            height: 68
            anchors.bottom: navigator.bottom

            color: ColorTheme.colors.dark4

            RowLayout
            {
                id: mainControls

                anchors.left: bottomBar.left
                anchors.leftMargin: 12
                anchors.right: bottomBar.right
                anchors.rightMargin: 12
                anchors.verticalCenter: bottomBar.verticalCenter

                visible: !d.searchActive

                spacing: 0

                LEDButton
                {
                    id: liveButton

                    text: qsTr("LIVE")
                    checked: controller.playingLive

                    font.pixelSize: 14
                    leftPadding: 12
                    rightPadding: 12

                    onClicked:
                    {
                        if (controller.playingLive)
                            controller.pause()
                        else
                            controller.playLive()
                    }
                }

                Item
                {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Row
                {
                    leftPadding: 8
                    rightPadding: 8
                    spacing: 8

                    ControlButton
                    {
                        icon.source: "image://skin/24x24/Outline/eye_view.svg"

                        onClicked:
                            objectsTypeSheet.open()
                    }

                    ControlButton
                    {
                        icon.source: "image://skin/24x24/Outline/selection.svg"
                        visible: false //< TODO: Implement timeline selection.
                    }
                }

                Item
                {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                ButtonBar
                {
                    ControlButton
                    {
                        id: zoomOutButton
                        icon.source: "image://skin/24x24/Outline/minus.svg"
                    }

                    ControlButton
                    {
                        id: zoomInButton
                        icon.source: "image://skin/24x24/Outline/plus.svg"
                    }

                    ZoomController
                    {
                        id: zoomController

                        zoomingState:
                        {
                            if (zoomInButton.pressed)
                                return ZoomController.ZoomingIn

                            return zoomOutButton.pressed
                                ? ZoomController.ZoomingOut
                                : ZoomController.Stopped
                        }

                        onZoomStep: (factor) =>
                            timeline.zoom(factor)
                    }
                }
            }

            RowLayout
            {
                id: searchControls

                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                visible: d.searchActive

                spacing: 8

                Text
                {
                    text: d.searchResults
                        ? `${d.searchIndex + 1} of ${d.searchResults}`
                        : qsTr("No results")

                    color: ColorTheme.colors.light10
                    font.pixelSize: 16
                    font.weight: 400
                    padding: 8

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                }

                ControlButton
                {
                    id: searchUp

                    icon.source: "image://skin/24x24/Outline/arrow_up.svg"
                    enabled: d.searchIndex < (d.searchResults - 1)

                    onClicked:
                        ++d.searchIndex
                }

                ControlButton
                {
                    id: searchDown

                    icon.source: "image://skin/24x24/Outline/arrow_down.svg"
                    enabled: d.searchIndex > 0

                    onClicked:
                        --d.searchIndex
                }
            }
        }
    }

    Item
    {
        id: noRightsPlaceholder

        anchors.fill: navigator
        visible: !d.canViewArchive

        Column
        {
            spacing: 24
            anchors.centerIn: parent

            ColoredImage
            {
                sourcePath: "image://skin/64x64/Outline/no_information.svg"
                sourceSize: Qt.size(64, 64)
                primaryColor: ColorTheme.colors.light10
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Column
            {
                spacing: 16
                width: noRightsPlaceholder.width

                Text
                {
                    text: qsTr("Not available")
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    color: ColorTheme.colors.light4
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text
                {
                    text: qsTr("You do not have permission<br>to view the archive",
                        "<br> is a line break")
                    font.pixelSize: 12
                    color: ColorTheme.colors.light12
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    ChunkProvider
    {
        id: cameraChunkProvider

        resource: controller.resource
    }

    CalendarPanel
    {
        id: calendarPanel

        horizontal: true
        position: timeline.positionMs
        displayOffset: controller.resourceHelper.displayOffset
        chunkProvider: cameraChunkProvider

        onPicked: controller.forcePosition(position, true)
    }

    Timeline.ObjectsTypeSheet
    {
        id: objectsTypeSheet
    }

    Item
    {
        id: content

        visible: false

        width: windowParams.availableWidth
        height: windowParams.availableHeight - header.height
        y: header.visible ? 0 : header.height

        Loader
        {
            id: informationLabelLoader
            anchors.right: parent.right
            anchors.rightMargin: 8
            opacity: d.cameraUiOpacity
            active: appContext.settings.showCameraInfo
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
            x: -windowParams.leftMargin
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

                    rightPadding: 8 + windowParams.rightMargin
                    leftPadding: 8 + windowParams.leftMargin
                    compact: videoScreen.height < 540
                    state: controller.dummyState

                    onButtonClicked:
                        controller.resourceHelper.cloudAuthorize()
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

            opacity: d.controlsOpacity
            visible: opacity > 0 && d.ptzMode

            onCloseButtonClicked: d.mode = VideoScreenUtils.VideoScreenMode.Navigation

            onMoveOnTapModeChanged:
            {
                if (ptzPanel.moveOnTapMode)
                {
                    moveOnTapOverlay.open()
                    video.fitToBounds()
                }
                else
                {
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

            x: -windowParams.leftMargin
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

                ptzPanel.moveOnTapMode = false
            }
        }
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

    Component.onDestruction:
        windowContext.ui.windowHelpers.exitFullscreen()
}

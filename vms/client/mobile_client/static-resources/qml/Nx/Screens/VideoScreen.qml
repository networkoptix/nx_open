// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import Nx.Common
import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Mobile.Ui.Sheets
import Nx.Models
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile
import nx.vms.client.mobile.timeline as Timeline
import nx.vms.common

import "private/VideoScreen"
import "private/VideoScreen/Fullscreen"
import "private/VideoScreen/Ptz"
import "private/VideoScreen/Timeline" as Timeline
import "private/VideoScreen/utils.js" as VideoScreenUtils

Page
{
    id: modernVideoScreen //< For the FT purposes must be different from the DeprecatedVideoScreen id.

    objectName: "videoScreen"

    property Resource initialResource

    property alias controller: controller
    property alias menu: menu

    readonly property QnCameraListModel defaultCamerasModel: QnCameraListModel {}
    property QnCameraListModel camerasModel: defaultCamerasModel

    property real targetTimestamp: -1

    property alias selectedObjectsType: objectsTypeSheet.selectedType
    property alias customRoiExists: video.roiController.customRoiExists

    // Whether this is an auxiliary video screen or the primary video screen.
    property bool auxiliary: false

    backgroundColor: "black"
    clip: false

    // TODO: FIXME! Control left handed / right handed layouts via settings.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    readonly property bool fullscreen: d.fullscreen

    states:
    [
        State
        {
            name: "withoutNavigator"

            when: !modernVideoScreen.ownsNavigator && !d.fullscreen

            AnchorChanges
            {
                target: navigationBar

                anchors.top: undefined
                anchors.bottom: modernVideoScreen.contentItem.bottom
                anchors.left: modernVideoScreen.contentItem.left
                anchors.right: modernVideoScreen.contentItem.right
            }

            PropertyChanges
            {
                cameraSwitcher.y: (navigationBar.y - cameraSwitcher.height) / 2

                bottomBar.color: ColorTheme.colors.dark6
                fullscreenControlsOverlay.visible: false
            }
        },

        State
        {
            name: "navigatorOnTheRight"

            when: modernVideoScreen.ownsNavigator
                && !LayoutController.isPortrait
                && !d.fullscreen

            AnchorChanges
            {
                target: navigatorProxyItem

                anchors.top: undefined
                anchors.left: undefined
                anchors.right: modernVideoScreen.contentItem.right
                anchors.bottom: undefined
            }

            AnchorChanges
            {
                target: navigationBar

                anchors.bottom: modernVideoScreen.contentItem.bottom
                anchors.left: modernVideoScreen.contentItem.left
                anchors.right: navigatorProxyItem.left
            }

            PropertyChanges
            {
                navigatorProxyItem.width: 360
                navigatorProxyItem.height: modernVideoScreen.height
                navigatorProxyItem.y: -modernVideoScreen.toolBar.height

                modernVideoScreen.toolBar.width: modernVideoScreen.width - navigatorProxyItem.width

                cameraSwitcher.y: (navigationBar.y - cameraSwitcher.height) / 2
                cameraSwitcher.width: modernVideoScreen.width - navigatorProxyItem.width

                bottomBar.color: ColorTheme.colors.dark6
                fullscreenControlsOverlay.visible: false
            }
        },

        State
        {
            name: "navigatorOnTheBottom"

            when: modernVideoScreen.ownsNavigator
                && LayoutController.isPortrait
                && !d.fullscreen

            AnchorChanges
            {
                target: navigationBar

                anchors.top: cameraSwitcher.bottom
                anchors.left: modernVideoScreen.contentItem.left
                anchors.right: modernVideoScreen.contentItem.right
            }

            AnchorChanges
            {
                target: navigatorProxyItem

                anchors.top: navigationBar.bottom
                anchors.right: modernVideoScreen.contentItem.right
                anchors.left: modernVideoScreen.contentItem.left
                anchors.bottom: modernVideoScreen.contentItem.bottom
            }

            PropertyChanges
            {
                fullscreenControlsOverlay.visible: false
            }
        },

        State
        {
            name: "fullscreen"

            when: d.fullscreen

            PropertyChanges
            {
                video.y: 0
                video.height: modernVideoScreen.height
                video.width: modernVideoScreen.width
                video.doubleClickZoom: false
                cameraSwitcher.height: modernVideoScreen.height

                fullscreenControlsOverlay.visible: true
                navigatorProxyItem.visible: false
                navigationBar.visible: false
                modernVideoScreen.header.visible: false
                bottomBar.visible: false
                bottomOverlayControls.visible: false
            }
        }
    ]

    VideoScreenController
    {
        id: controller

        videoScreen: modernVideoScreen

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

        onOfflineChanged:
        {
            if (offline)
            {
                offlineStatusDelay.restart()
                ptzSheet.ptz.moveOnTapMode = false
            }
            else
            {
                offlineStatusDelay.stop()
                d.showOfflineStatus = false
            }
        }

        onPlayingLiveChanged:
        {
            if (!playingLive)
                d.mode = VideoScreenUtils.VideoScreenMode.Navigation
        }

        resourceHelper.onResourceRemoved:
        {
            if (modernVideoScreen.activePage)
                Workflow.popCurrentScreen()
        }

        resourceHelper.onResourceChanged:
        {
            video.roiController.clearCustomRoi()
            d.mode = VideoScreenUtils.VideoScreenMode.Navigation
        }
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

        property int mode: VideoScreenUtils.VideoScreenMode.Navigation
        readonly property bool ptzMode: mode === VideoScreenUtils.VideoScreenMode.Ptz

        property bool restorePortraitScreenOrientation: false
        property bool fullscreen: false

        Connections
        {
            target: LayoutController

            function onIsPortraitChanged()
            {
                // The screen orientation changed due phone rotation. Orientation lock is off.
                if (d.fullscreen && LayoutController.isPortrait)
                {
                    d.restorePortraitScreenOrientation = false
                    d.fullscreen = false
                }
            }
        }

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

        icon.source: lp("/images/more_vert.png")
        onClicked:
        {
            menu.open()
        }
    }

    // Anchor item for the kebab menu in non-fullscreen state. Defaults to the toolBar's
    // own kebab IconButton, but can be overridden when this VideoScreen is embedded in
    // another screen whose own toolbar hosts the visible kebab (e.g. tablet layout in
    // ResourcesScreen, where modernVideoScreen.toolBar is hidden).
    property Item menuAnchor: menuButton

    property MotionAreaButton motionAreaButton: MotionAreaButton
    {
        text: qsTr("Area")
        icon.source: lp("/images/close.png")

        onClicked: video.roiController.clearCustomRoi()
    }

    title: cameraSwitcher.resourceName
    titleLabelOpacity: Math.abs(cameraSwitcher.transitionFraction * 2 - 1)
    titleControls:
        [
            LayoutItemProxy
            {
                anchors.verticalCenter: parent.verticalCenter
                target: modernVideoScreen.motionAreaButton
                visible: modernVideoScreen.activePage
                    && (selectedObjectsType == Timeline.ObjectsLoader.ObjectsType.motion
                        || selectedObjectsType == Timeline.ObjectsLoader.ObjectsType.analytics)
                    && video.roiController.customRoiExists
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

        parent: modernVideoScreen.state === "fullscreen"
            ? fullscreenControlsOverlay.menuButtonControl
            : modernVideoScreen.menuAnchor

        x: parent.width - width
        y: parent.height + 8

        MenuItem
        {
            id: cameraSettingsMenuItem

            text: qsTr("Camera Settings")

            onTriggered:
            {
                Workflow.openCameraSettingsScreen(
                    controller.mediaPlayer,
                    controller.resourceHelper.audioSupported,
                    controller.audioController)
            }
        }

        MenuItem
        {
            id: informationMenuItem

            text: qsTr("Camera Info")
            checkable: true
            checked: appContext.settings.showCameraInfo

            onTriggered:
                appContext.settings.showCameraInfo = !appContext.settings.showCameraInfo
        }

        MenuItem
        {
            id: ptzMenuItem

            text: qsTr("PTZ Mode")
            visible: ptzSheet.available
            height: visible ? implicitHeight : 0

            enabled: controller.playingLive
            showDisabled: true
            disabledDescription: qsTr("Live mode only")

            onTriggered:
            {
                d.mode = VideoScreenUtils.VideoScreenMode.Ptz
                video.to1xScale()
            }
        }

        MenuItem
        {
            id: bookmarksMenuItem

            text: qsTr("Bookmarks")
            visible: (controller.systemContext?.hasViewBookmarksPermission
                && !(controller.resource?.flags & ResourceFlag.cross_system)) ?? false
            height: visible ? implicitHeight : 0

            onTriggered:
                Workflow.openEventSearchScreen(/*push*/ true, controller.resource.id, camerasModel)
        }

        MenuItem
        {
            id: objectsMenuItem

            text: qsTr("Objects")
            visible: (controller.systemContext?.hasSearchObjectsPermission
                && !(controller.resource?.flags & ResourceFlag.cross_system)) ?? false
            height: visible ? implicitHeight : 0

            onTriggered:
            {
                Workflow.openEventSearchScreen(
                    /*push*/ true, controller.resource.id, camerasModel, true)
            }
        }

        MenuItem
        {
            id: exportMenuItem

            text: qsTr("Export...")

            visible: mediaDownloadBackend.isDownloadAvailable
            height: visible ? implicitHeight : 0
            enabled: !controller.playingLive
            showDisabled: true
            disabledDescription: qsTr("Archive mode only")

            onTriggered:
                downloadMediaSheet.open()
        }
    }

    ScalableVideo
    {
        id: video

        readonly property bool shown: dummyLoader.status != Loader.Ready

        readonly property bool supportsRoi:
        {
            const type = modernVideoScreen.selectedObjectsType

            return d.hasArchive && !video.fisheyeMode && !d.ptzMode
                && (type === Timeline.ObjectsLoader.ObjectsType.analytics
                    || type === Timeline.ObjectsLoader.ObjectsType.motion)
        }

        width: parent?.width ?? 800
        height: width / (16.0 / 9.0)

        visible: video.shown

        resourceHelper: controller.resourceHelper
        mediaPlayer: controller.mediaPlayer
        videoCenterHeightOffsetFactor: 1 / 3

        showMotion: !d.ptzMode
            && modernVideoScreen.selectedObjectsType === Timeline.ObjectsLoader.ObjectsType.motion

        roiController
        {
            // At this point we're using a legacy ROI design and a legacy ROI drawing mechanism
            // crudely adapted for the new needs.

            allowDrawing: true
            motionSearchMode: true

            enabled: video.supportsRoi
            visible: video.supportsRoi
        }

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
            target: video.roiController

            function onEmptyRoiCleared()
            {
                banner.showText(qsTr("Invalid custom area. Please draw a correct one."))
            }
        }
    }

    CameraSwitcher
    {
        id: cameraSwitcher

        width: parent.width
        height: width / (16.0 / 9.0)
        spacing: 8

        videoItem: video
        camerasModel: modernVideoScreen.camerasModel
        controller: controller

        interactive: !video.roiController.drawingRoi && Math.abs(video.scale - 1.0) < 0.01
    }

    Item
    {
        id: actionVisualizerContainer

        anchors.top: cameraSwitcher.top
        anchors.horizontalCenter: cameraSwitcher.horizontalCenter
        anchors.margins: 8

        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }

    NxDotPreloader
    {
        anchors.centerIn: cameraSwitcher
        color: ColorTheme.colors.light16
        dotRadius: 6
        spacing: 8

        running: controller.preloaderRequired
    }

    FullscreenControlsOverlay
    {
        id: fullscreenControlsOverlay

        anchors.fill: parent
        controller: modernVideoScreen.controller

        cameraTitle: controller.resourceHelper.qualifiedResourceName
        cameraTimestampText: timeline.labelFormatter.cameraTimestamp(
            controller.mediaPlayer.displayedPosition, timeline.timeZone)

        showPlaybackControls: d.hasArchive

        actionsButtonEnabled: controller.playingLive
        actionsButtonVisible: actionSheet.hasActions

        onBackButtonClicked:
        {
            d.restorePortraitScreenOrientation = false
            Workflow.popCurrentScreen()
            d.fullscreen = false
        }

        onExitFullscreenButtonClicked:
        {
            d.fullscreen = false
            if (d.restorePortraitScreenOrientation)
            {
                windowContext.ui.windowHelpers.setScreenOrientation(Qt.PortraitOrientation)
                d.restorePortraitScreenOrientation = false
            }
        }

        onActionsButtonClicked:
        {
            actionSheet.open()
        }

        onMenuButtonClicked:
        {
            menu.open()
        }
    }

    Rectangle
    {
        id: navigationBar

        implicitHeight: navigationBarContent.height + navigationBarContent.contentMargin * 2

        color: LayoutController.isTabletLayout && !modernVideoScreen.activePage
            ? "transparent"
            : ColorTheme.colors.dark4

        readonly property bool hasChunkNavigation:
            modernVideoScreen.selectedObjectsType !== Timeline.ObjectsLoader.ObjectsType.bookmarks

        Item
        {
            id: navigationBarContent

            readonly property real contentMargin: 12
            readonly property real availableWidth: navigationBar.width - contentMargin * 2

            component ExpandCollapseAnimation: NumberAnimation { duration: 150 }

            anchors.verticalCenter: navigationBar.verticalCenter
            anchors.horizontalCenter: navigationBar.horizontalCenter
            height: navigationBarLayout.implicitHeight

            RowLayout
            {
                id: navigationBarLayout

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

                        enabled: d.hasArchive
                            && navigationBar.hasChunkNavigation
                            && NxGlobals.isValidTime(controller.prevChunkMs)

                        opacity: navigationBar.hasChunkNavigation ? 1 : 0

                        onClicked:
                            controller.jumpToPreviousChunk()
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

                        SpeedControl
                        {
                            id: speedControl

                            expandedWidth: navigationBarContent.availableWidth
                                - playPauseButton.width
                                - playBar.spacing

                            forced1x: controller.playingLive
                            paused: !controller.playing

                            color: playBar.backgroundColor
                            radius: playBar.roundingRadius

                            onMoved:
                            {
                                if (pausable)
                                {
                                    if (speed == 0)
                                        controller.pause()
                                    else
                                        controller.play()
                                }

                                controller.speed = speedControl.speed
                            }
                        }

                        onEnabledChanged:
                        {
                            if (!enabled)
                                speedControl.expanded = false
                        }
                    }

                    ControlButton
                    {
                        id: nextChunkButton

                        icon.source: "image://skin/24x24/Outline/chunk_future.svg"

                        enabled: d.hasArchive && navigationBar.hasChunkNavigation
                            && !controller.playingLive

                        opacity: navigationBar.hasChunkNavigation ? 1 : 0

                        onClicked:
                            controller.jumpToNextChunk()
                    }
                }

                Item
                {
                    id: actionButtonContainer

                    Layout.preferredWidth: childrenRect.width
                    Layout.preferredHeight: childrenRect.height

                    enabled: controller.playingLive
                    visible: actionSheet.hasActions
                }
            }
        }
    }

    ProxyItem
    {
        id: navigatorProxyItem

        implicitWidth: 360
        target: navigator
        visible: modernVideoScreen.ownsNavigator
    }

    // Whether the given video screen owns the navigator item. If the video screen owns navigator
    // item, it should position it itself, otherwise the navigator is positioned by the parent
    // item of the video screen.
    property bool ownsNavigator: true

    property Item navigatorItem: Item
    {
        id: navigator

        DataTimeline
        {
            id: timeline

            width: navigator.width
            anchors.top: navigator.top
            anchors.bottom: bottomBar.top
            visible: d.hasArchive

            chunkProvider: cameraChunkProvider
            objectsType: modernVideoScreen.selectedObjectsType

            interactive: !objectsTypeSheet.opened && !timelineObjectSheet.opened
                && !ptzSheet.opened && !actionSheet.opened

            timeZone: video.resourceHelper.timeZone

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

            objectsType: timeline.objectsType
            dateFormatter: ((timestampMs) => timeline.labelFormatter.objectTimestamp(
                timestampMs, timeline.timeZone))
        }

        Rectangle
        {
            id: bottomBar

            width: navigator.width
            height: 68
            anchors.bottom: navigator.bottom
            visible: d.hasArchive

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

                    text: "LIVE" //< Intentionally not translatable.
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
                    id: zoomButtons

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
        }

        ArchivePlaceholder
        {
            id: archivePlaceholder

            anchors.fill: navigator
            color: ColorTheme.colors.mobileTimeline.background

            canViewArchive: d.canViewArchive
            hasArchive: d.hasArchive
            loading: cameraChunkProvider.loading
        }
    }

    ChunkProvider
    {
        id: cameraChunkProvider

        resource: controller.resource
        motionFilter: video.roiController.motionFilter
        analyticsRoi: video.roiController.customRoi
    }

    Timer
    {
        id: chunkProviderUpdateTimer

        interval: 30000
        triggeredOnStart: true
        running: d.applicationActive && !!controller.resource
        repeat: true

        onTriggered:
            cameraChunkProvider.update()
    }

    CalendarPanel
    {
        id: calendarPanel

        horizontal: true
        position: timeline.startTimeMs
        timeZone: controller.resourceHelper.timeZone
        chunkProvider: cameraChunkProvider

        onPicked: (position) =>
        {
            const oneHourMs = 60 * 60 * 1000
            const oneDayMs = 24 * oneHourMs

            // Navigate to the day with one hour margins on each side.
            timeline.setWindow(position - oneHourMs, oneDayMs + oneHourMs * 2)
        }
    }

    Timeline.ObjectsTypeSheet
    {
        id: objectsTypeSheet

        onObjectsTypeClicked:
        {
            if (!modernVideoScreen.auxiliary)
                appContext.settings.selectedObjectsType = objectsTypeSheet.selectedType
        }
    }

    ActionSheet
    {
        id: actionSheet

        resource: controller.resource
        externalButtonContainer: actionButtonContainer
        externalVisualizerContainer: actionVisualizerContainer

        Binding on externalMode
        {
            when: modernVideoScreen.state === "fullscreen"
            value: false
        }
    }

    MediaDownloadBackend
    {
        id: mediaDownloadBackend
        resource: controller.resource
        onErrorOccurred: Workflow.openStandardPopup(title, description)
    }

    DownloadMediaDurationSheet
    {
        id: downloadMediaSheet
        onDurationPicked: function(duration)
        {
            mediaDownloadBackend.downloadVideo(timeline.positionMs, duration)
        }
    }

    Item
    {
        id: content

        anchors.fill: video
        parent: video.parent

        Rectangle
        {
            id: bottomOverlayControls

            height: 56
            visible: video.shown

            anchors.bottom: content.bottom
            anchors.right: content.right
            anchors.left: content.left

            gradient: Gradient
            {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: ColorTheme.colors.dark1 }
            }

            Text
            {
                id: timestampLabel

                anchors.left: bottomOverlayControls.left
                anchors.leftMargin: 14
                anchors.bottom: bottomOverlayControls.bottom
                anchors.bottomMargin: 9

                font.pixelSize: 16
                font.weight: Font.Medium

                color: ColorTheme.colors.light4

                text: timeline.labelFormatter.cameraTimestamp(
                    controller.mediaPlayer.displayedPosition, timeline.timeZone)
            }

            ControlButton
            {
                id: fullscreenButton

                anchors.right: bottomOverlayControls.right
                anchors.bottom: bottomOverlayControls.bottom
                width: 48
                height: 48
                foregroundColor: ColorTheme.colors.light4
                backgroundColor: "transparent"
                icon.source: "image://skin/24x24/Outline/full_screen.svg"

                onClicked:
                {
                    d.fullscreen = true
                    if (LayoutController.isPortrait)
                    {
                        // If fullscreen mode was entered from portrait orientation - always restore
                        // portrait orientation back, to avoid orientation change with orientation
                        // lock enabled simply by going to the fullscreen and back.
                        d.restorePortraitScreenOrientation = true
                        windowContext.ui.windowHelpers.setScreenOrientation(
                            Qt.LandscapeOrientation)
                    }
                }
            }
        }

        Loader
        {
            id: informationLabelLoader

            y: 8
            anchors.right: parent.right
            anchors.rightMargin: 8

            active: appContext.settings.showCameraInfo && !d.cameraWarningVisible

            sourceComponent: InformationLabel
            {
                videoScreenController: controller
            }
        }

        Loader
        {
            id: dummyLoader

            anchors.fill: parent

            visible: active
            active: d.cameraWarningVisible

            sourceComponent: Component
            {
                VideoDummy
                {
                    rightPadding: 8 + windowParams.rightMargin
                    leftPadding: 8 + windowParams.leftMargin
                    compact: cameraSwitcher.height < 200
                    centered: true
                    state: controller.dummyState

                    onButtonClicked:
                        controller.resourceHelper.cloudAuthorize()
                }
            }
        }

        PtzSheet
        {
            id: ptzSheet

            preloaders.parent: video
            preloaders.height: video.fitSize ? video.fitSize.height : 0
            preloaders.x: (cameraSwitcher.width - preloaders.width) / 2
            preloaders.y: (cameraSwitcher.height - preloaders.height) / 3

            controller.resource: controller.resource
            customRotation: controller.resourceHelper.customRotation

            opacity: d.controlsOpacity
            visible: opacity > 0 && d.ptzMode

            onClosed:
                d.mode = VideoScreenUtils.VideoScreenMode.Navigation

            ptz.onMoveOnTapModeChanged:
            {
                if (ptz.moveOnTapMode)
                {
                    moveOnTapOverlay.open()
                    video.fitToBounds()
                }
                else
                {
                    moveOnTapOverlay.close()
                }
            }

            Overlay.modeless: Item {}
        }

        CustomPopupDimmer
        {
            id: ptzSheetDimmer

            popup: ptzSheet
            parent: modernVideoScreen.contentItem

            anchors.top: (parent === modernVideoScreen.contentItem) ? navigationBar.top : undefined
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
        }

        PtzViewportMovePreloader
        {
            id: preloader

            parent: modernVideoScreen
            visible: false
        }

        MoveOnTapOverlay
        {
            id: moveOnTapOverlay

            x: -windowParams.leftMargin
            width: modernVideoScreen.width
            height: parent.height
            parent: modernVideoScreen
            opacity: ptzSheet.opened ? 0.0 : 1.0

            onClicked:
            {
                if (controller.resourceHelper.fisheyeParams.enabled || !video)
                    return

                var mapped = contentItem.mapToItem(video, pos.x, pos.y)
                var data = video.getMoveViewportData(mapped)
                if (!data)
                    return

                ptzSheet.ptz.moveViewport(data.viewport, data.aspect)
                preloader.pos = contentItem.mapToItem(preloader.parent, pos.x, pos.y)
                preloader.visible = true
            }

            onVisibleChanged:
            {
                if (moveOnTapOverlay.visible)
                    return

                ptzSheet.ptz.moveOnTapMode = false
            }
        }
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

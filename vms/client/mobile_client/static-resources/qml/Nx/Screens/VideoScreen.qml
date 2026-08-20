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

import "private"
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

    // Places for the side panel buttons of the hosting screen, at the edges of the playback
    // controls row. They keep the buttons in line with the controls instead of floating over them.
    property alias leftPanelButtonSlot: leftPanelButtonSlot
    property alias rightPanelButtonSlot: rightPanelButtonSlot

    // Whether that screen shows those buttons at the moment. The bar keeps the room only for the
    // ones which ask for it.
    property bool leftPanelButtonWanted: false
    property bool rightPanelButtonWanted: false

    signal backClicked()

    backgroundColor: "black"
    clip: false

    LayoutMirroring.enabled: appContext.settings.leftHandedMode
    LayoutMirroring.childrenInherit: true

    states:
    [
        State
        {
            name: "withoutNavigator"

            when: !modernVideoScreen.ownsNavigator && !LayoutController.fullscreen

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
                // Must not go under the navigation bar, which keeps the video controls and the
                // side panel buttons of the parent screen.
                cameraSwitcher.height:
                    modernVideoScreen.contentItem.height - navigationBar.height
                cameraSwitcher.y: 0
                cameraSwitcher.backgroundColor: modernVideoScreen.backgroundColor

                bottomBar.color: ColorTheme.colors.dark6
                fullscreenControlsOverlay.visible: false
            }
        },

        State
        {
            name: "navigatorOnTheRight"

            when: modernVideoScreen.ownsNavigator
                && !LayoutController.isCompact
                && !LayoutController.fullscreen

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

                // The navigator takes the header area as well, which the narrowed tool bar leaves
                // free. The tool bar keeps its height while hidden, so it cannot measure that
                // area: the header, which the parent screen may collapse, is the actual offset.
                navigatorProxyItem.y: -modernVideoScreen.header.height

                modernVideoScreen.toolBar.width: modernVideoScreen.width - navigatorProxyItem.width

                // As in the state without the navigator: the video area takes the whole space left
                // of the navigator and above the navigation bar, and fits the picture inside
                // itself. Sizing the area to the picture instead would leave the space around it
                // to the screen background, which reads as a taller navigation bar.
                cameraSwitcher.height:
                    modernVideoScreen.contentItem.height - navigationBar.height
                cameraSwitcher.y: 0
                cameraSwitcher.width: modernVideoScreen.width - navigatorProxyItem.width
                cameraSwitcher.backgroundColor: modernVideoScreen.backgroundColor

                bottomBar.color: ColorTheme.colors.dark6
                fullscreenControlsOverlay.visible: false
            }
        },

        State
        {
            name: "navigatorOnTheBottom"

            when: modernVideoScreen.ownsNavigator
                && LayoutController.isCompact
                && !LayoutController.fullscreen

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

            when: LayoutController.fullscreen

            PropertyChanges
            {
                video.y: 0
                video.height: modernVideoScreen.height
                video.width: modernVideoScreen.width
                video.doubleClickZoom: false
                cameraSwitcher.height: modernVideoScreen.height

                fullscreenControlsOverlay.visible: !d.ptzMode
                navigatorProxyItem.visible: false
                navigationBar.visible: false
                bottomBar.visible: false
                bottomOverlayControls.visible: false
            }
        }
    ]

    // The header comes from the base Page component and is managed by QQuickPage, so relying on
    // the fullscreen state's restoreEntryValues to bring it back is fragile: when this screen sits
    // in the background under another fullscreen screen and the device is rotated, the restore may
    // be lost and the header stays hidden. Driving the visibility with a plain binding keeps it a
    // pure function of the fullscreen state and cannot get stuck.
    Binding
    {
        target: modernVideoScreen.header
        property: "visible"
        value: !LayoutController.fullscreen
        restoreMode: Binding.RestoreBindingOrValue
    }

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
                ptz.reset()
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

        property bool forceVideoPause: downloadMediaSheet.opened

        // Whether the displayed video (with the camera rotation applied) is vertical.
        readonly property bool verticalVideo:
        {
            const aspectRatio = controller.mediaPlayer.aspectRatio
            if (aspectRatio <= 0)
                return false //< Not known yet; the default (forced landscape) applies.

            return Geometry.isRotated90(controller.resourceHelper.customRotation)
                ? aspectRatio > 1
                : aspectRatio < 1
        }

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

        readonly property bool hasChunkNavigation:
            modernVideoScreen.selectedObjectsType !== Timeline.ObjectsLoader.ObjectsType.bookmarks

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

    rightControl: ToolBarButton
    {
        id: menuButton

        icon.source: "image://skin/24x24/Outline/more.svg?primary=%1"
            .arg(StyleHints.foregroundColorName)
        onClicked: menu.open()
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
    titleUnderlineVisible: siteToolTip.available
    titleControls:
        [
            LayoutItemProxy
            {
                anchors.verticalCenter: parent.verticalCenter
                target: modernVideoScreen.motionAreaButton
                visible: modernVideoScreen.toolBar.visible
                    && modernVideoScreen.activePage
                    && (selectedObjectsType == Timeline.ObjectsLoader.ObjectsType.motion
                        || selectedObjectsType == Timeline.ObjectsLoader.ObjectsType.analytics)
                    && video.roiController.customRoiExists
            }
        ]

    toolBar.contentItem
    {
        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: true
        clip: false
    }

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

        resource: cameraSwitcher.resource
    }

    gradientToolbarBackground: true

    VideoScreenBanner
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

    SiteToolTip
    {
        id: siteToolTip

        toolBar: modernVideoScreen.toolBar
        text: controller.resourceHelper.crossSystemName
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
            visible: ptz.available
            height: visible ? implicitHeight : 0

            enabled: controller.playingLive
            showDisabled: true
            disabledDescription: qsTr("Live mode only")

            onTriggered:
            {
                if (!d.ptzMode)
                    d.mode = VideoScreenUtils.VideoScreenMode.Ptz
                else
                    d.mode = VideoScreenUtils.VideoScreenMode.Navigation
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
        height: Math.min(width / (16.0 / 9.0), (parent?.height ?? 600))

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

        onClicked:
        {
            if (LayoutController.fullscreen)
                fullscreenControlsOverlay.toggle()
        }

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

        anchors.left: parent.left

        width: parent.width
        height: width / (16.0 / 9.0)
        spacing: 8

        videoItem: video
        camerasModel: modernVideoScreen.camerasModel
        controller: controller

        interactive: !video.roiController.drawingRoi
            && !(d.ptzMode && d.fullscreen)
            && video.zoomedOut
    }

    Item
    {
        id: actionVisualizerContainer

        anchors.top: cameraSwitcher.top
        anchors.horizontalCenter: cameraSwitcher.horizontalCenter
        anchors.margins: 8

        width: Math.min(parent.width - anchors.margins * 2, implicitWidth)

        implicitWidth: children[0]?.implicitWidth ?? 0
        implicitHeight: children[0]?.implicitHeight ?? 0
    }

    Rectangle
    {
        id: videoPreloaderShade

        anchors.fill: cameraSwitcher

        color: ColorTheme.transparent(ColorTheme.colors.dark1, 0.5)
        opacity: controller.preloaderRequired ? 1 : 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 250 }}

        NxDotPreloader
        {
            id: videoPreloader

            anchors.centerIn: parent
            color: ColorTheme.colors.light10
            dotRadius: 6
            spacing: 8

            running: videoPreloaderShade.opacity > 0
        }
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
        hasChunkNavigation: d.hasChunkNavigation

        hasActionButton: actionSheet.hasActions

        onBackButtonClicked:
        {
            if (!modernVideoScreen.StackView.view)
            {
                modernVideoScreen.backClicked()
                return
            }

            Workflow.popCurrentScreen()
        }

        onExitFullscreenButtonClicked: LayoutController.exitFullscreen()

        onMenuButtonClicked:
        {
            menu.open()
        }
    }

    Rectangle
    {
        id: navigationBar
        implicitHeight: leftPanelButtonSlot.docked || rightPanelButtonSlot.docked
            ? 80
            : navigationBarContent.height + navigationBarContent.contentMargin * 2
        color: modernVideoScreen.backgroundColor

        // The playback controls are the primary content of the bar: once it gets too narrow to
        // keep them between the panel buttons, the buttons give way.
        readonly property bool fitsPanelButtons: width >= navigationBarContent.requiredWidth
            + Math.max(navigationBarContent.contentMargin, leftPanelButtonSlot.reservedWidth)
            + Math.max(navigationBarContent.contentMargin, rightPanelButtonSlot.reservedWidth)

        // A place at the edge of the bar for a side panel button of the hosting screen. The button
        // is reparented here by that screen; the slot goes away when the bar has no width to
        // spare, leaving it to the playback controls.
        component PanelButtonSlot: Item
        {
            property bool wanted: false
            property bool fits: true

            readonly property Item button: children.length ? children[0] : null
            readonly property bool docked: !!button
            readonly property bool active: docked && wanted && fits
            readonly property real edgeMargin: 20
            readonly property real contentGap: 20

            readonly property real reservedWidth: docked && wanted
                ? edgeMargin + implicitWidth + contentGap
                : 0

            readonly property real occupiedWidth: active ? reservedWidth : 0
            LayoutMirroring.enabled: false

            implicitWidth: 56
            implicitHeight: 56

            visible: active
            width: active ? implicitWidth : 0
            height: active ? implicitHeight : 0
        }

        PanelButtonSlot
        {
            id: leftPanelButtonSlot

            wanted: modernVideoScreen.leftPanelButtonWanted
            fits: navigationBar.fitsPanelButtons

            anchors.left: navigationBar.left
            anchors.leftMargin: edgeMargin
            anchors.verticalCenter: navigationBar.verticalCenter
        }

        PanelButtonSlot
        {
            id: rightPanelButtonSlot

            wanted: modernVideoScreen.rightPanelButtonWanted
            fits: navigationBar.fitsPanelButtons

            anchors.right: navigationBar.right
            anchors.rightMargin: edgeMargin
            anchors.verticalCenter: navigationBar.verticalCenter
        }

        Item
        {
            id: navigationBarContent

            readonly property real contentMargin: 12

            readonly property real leftInset:
                Math.max(contentMargin, leftPanelButtonSlot.occupiedWidth)
            readonly property real rightInset:
                Math.max(contentMargin, rightPanelButtonSlot.occupiedWidth)

            readonly property real availableWidth: navigationBar.width - leftInset - rightInset

            readonly property real requiredWidth: playbackControlsWidth
                + calendarButton.implicitWidth
                + (actionSheet.hasActions ? actionButtonContainer.Layout.preferredWidth : 0)

            // Whether the bar is too narrow even for the controls alone, with the panel buttons
            // already given way. The secondary controls - the calendar and the camera actions -
            // then move into the overflow menu, which takes the place of one of them. With no
            // actions to hide there is nothing to win: the menu button is as wide as the calendar.
            readonly property bool overflow: actionSheet.hasActions
                && navigationBar.width - contentMargin * 2 < requiredWidth

            // The space the controls occupy with the speed control collapsed - the expanded speed
            // control takes exactly it - and the space of the playback controls alone.
            property real collapsedControlsWidth: 0
            property real playbackControlsWidth: 0
            readonly property bool controlsSettled: !speedControl.expanded
                && speedControl.width <= speedControl.collapsedWidth

            Binding
            {
                target: navigationBarContent
                property: "collapsedControlsWidth"
                value: navigationBarLayout.implicitWidth
                when: navigationBarContent.controlsSettled
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: navigationBarContent
                property: "playbackControlsWidth"
                value: playbackControls.implicitWidth
                when: navigationBarContent.controlsSettled
                restoreMode: Binding.RestoreNone
            }

            component ExpandCollapseAnimation: NumberAnimation
            {
                duration: 150
                easing.type: Easing.OutCubic
            }

            anchors.verticalCenter: navigationBar.verticalCenter
            anchors.horizontalCenter: navigationBar.horizontalCenter
            height: navigationBarLayout.implicitHeight

            RowLayout
            {
                id: navigationBarLayout

                readonly property real leftControlsWidth:
                    (calendarButton.visible ? calendarButton.width : 0)
                        + playbackControls.leftPadding
                        + prevChunkButton.width
                        + playbackControls.spacing

                readonly property real rightControlsWidth: playbackControls.spacing
                    + nextChunkButton.width
                    + playbackControls.rightPadding
                    + (actionButtonContainer.visible ? actionButtonContainer.width : 0)
                    + (overflowButton.visible ? overflowButton.width : 0)

                property real centerOffset:
                    (navigationBarContent.leftInset - navigationBarContent.rightInset) / 2
                        + (speedControl.expanded
                            ? (rightControlsWidth - leftControlsWidth) / 2
                            : 0)

                Behavior on centerOffset { ExpandCollapseAnimation {} }

                anchors.horizontalCenter: navigationBarContent.horizontalCenter
                anchors.horizontalCenterOffset: centerOffset

                spacing: 0

                ControlButton
                {
                    id: calendarButton

                    icon.source: "image://skin/24x24/Solid/calendar.svg"
                    visible: !navigationBarContent.overflow
                    enabled: d.hasArchive && !speedControl.expanded
                    opacity: speedControl.expanded ? 0 : 1

                    Behavior on opacity { ExpandCollapseAnimation {} }

                    onClicked:
                        calendarPanel.open()
                }

                Row
                {
                    id: playbackControls

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
                            && d.hasChunkNavigation
                            && !speedControl.expanded
                            && NxGlobals.isValidTime(controller.prevChunkMs)

                        opacity: (d.hasChunkNavigation && !speedControl.expanded)
                            ? 1 : 0

                        Behavior on opacity { ExpandCollapseAnimation {} }

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

                            // Takes over the whole space the controls occupied before it expanded,
                            // and never more than the bar can give away.
                            expandedWidth: Math.min(
                                    navigationBarContent.availableWidth,
                                    navigationBarContent.collapsedControlsWidth)
                                - playPauseButton.width
                                - playBar.spacing

                            forced1x: controller.playingLive
                            paused: !controller.playing

                            color: playBar.backgroundColor
                            radius: playBar.roundingRadius

                            LayoutMirroring.enabled: false
                            LayoutMirroring.childrenInherit: true

                            onMoved:
                                controller.setSpeed(speed)
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

                        enabled: d.hasArchive && d.hasChunkNavigation
                            && !controller.playingLive
                            && !speedControl.expanded

                        opacity: (d.hasChunkNavigation && !speedControl.expanded) ? 1 : 0

                        Behavior on opacity { ExpandCollapseAnimation {} }

                        onClicked:
                            controller.jumpToNextChunk()
                    }
                }

                Item
                {
                    id: actionButtonContainer

                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44

                    visible: actionSheet.hasActions && !navigationBarContent.overflow
                    enabled: !speedControl.expanded
                    opacity: speedControl.expanded ? 0 : 1

                    Behavior on opacity { ExpandCollapseAnimation {} }
                }

                // Holds the controls which do not fit into the narrow bar as buttons of their own.
                ControlButton
                {
                    id: overflowButton

                    icon.source: "image://skin/24x24/Outline/menu.svg"
                    visible: navigationBarContent.overflow
                    enabled: !speedControl.expanded
                    opacity: speedControl.expanded ? 0 : 1

                    Behavior on opacity { ExpandCollapseAnimation {} }

                    onClicked:
                        overflowMenu.open()

                    Menu
                    {
                        id: overflowMenu

                        // Opens up from the button: the bar is at the bottom of the screen.
                        x: overflowButton.width - width
                        y: -height - 8

                        MenuItem
                        {
                            text: qsTr("Calendar")
                            enabled: d.hasArchive
                            showDisabled: true

                            onTriggered:
                                calendarPanel.open()
                        }

                        MenuItem
                        {
                            text: qsTr("Actions")
                            visible: actionSheet.hasActions
                            height: visible ? implicitHeight : 0

                            // Dimmed exactly as the action button in the bar is while the actions
                            // cannot be triggered.
                            enabled: actionSheet.available
                            showDisabled: true
                            disabledDescription: qsTr("Live mode only")

                            onTriggered:
                                actionSheet.open()
                        }
                    }
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

        readonly property string displayedDataType:
        {
            if (!timeline.visible)
                return ""

            switch(timeline.objectsType)
            {
                case Timeline.ObjectsLoader.ObjectsType.motion: return qsTr("Motion")
                case Timeline.ObjectsLoader.ObjectsType.bookmarks: return qsTr("Bookmarks")
                case Timeline.ObjectsLoader.ObjectsType.analytics: return qsTr("Objects")
                default: return ""
            }
        }

        DataTimeline
        {
            id: timeline

            width: navigator.width
            anchors.top: navigator.top
            anchors.bottom: bottomBar.top
            visible: d.hasArchive

            chunkProvider: cameraChunkProvider
            objectsType: modernVideoScreen.selectedObjectsType

            interactive: !objectsTypeSheet.opened
                && !objectSelectionSheet.opened
                && !objectActionsMenu.opened
                && !ptz.active
                && !actionSheet.opened
                && !downloadMediaSheet.opened
                && !mainWindow.banner.modalActive

            timeZone: video.resourceHelper.timeZone

            onDetailsRequested: (data, tile) =>
            {
                if (!data?.perObjectData || !tile)
                    return

                const invokerRect = objectActionsMenu.parent.mapFromItem(tile,
                    0, 0, tile.width, tile.height)

                objectActionsMenu.resource = controller.resource
                objectActionsMenu.objectsType = timeline.objectsType
                objectActionsMenu.multiObjectData = data
                objectActionsMenu.adjustPosition(invokerRect, objectActionsMenu.indent)
                objectActionsMenu.open()
            }
        }

        Timeline.ObjectSelectionSheet
        {
            id: objectSelectionSheet

            objectsType: objectActionsMenu.objectsType
            tileHeight: timeline.tileHeight
            preferredEdge: StyleHints.preferredSheetEdge
        }

        Timeline.ObjectActionsMenu
        {
            id: objectActionsMenu

            property real indent: 8
            selector: objectSelectionSheet
            parent: modernVideoScreen.contentItem
            minimumDownloadDurationMs: timeline.minimumDurationMs
        }

        Rectangle
        {
            id: bottomBar

            width: navigator.width

            // Wherever the navigator stands beside the video, the two bottom bars are next to
            // each other and must read as one row, so this one follows the video controls.
            height: navigationBar.height

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
                        icon.source:
                        {
                            switch (timeline.objectsType)
                            {
                                case Timeline.ObjectsLoader.ObjectsType.motion:
                                    return "image://skin/24x24/Outline/motion.svg"

                                case Timeline.ObjectsLoader.ObjectsType.bookmarks:
                                    return "image://skin/24x24/Outline/bookmark.svg"

                                case Timeline.ObjectsLoader.ObjectsType.analytics:
                                    return "image://skin/24x24/Outline/object.svg"
                            }

                            console.assert(false, `Unknown objectsType (${timeline.objectsType})`)
                            return "image://skin/24x24/Outline/eye_view.svg"
                        }

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

        preferredEdge: StyleHints.preferredSheetEdge

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
        preferredEdge: StyleHints.preferredSheetEdge
        overlayStyle: modernVideoScreen.state === "fullscreen"
        externalVisualizerContainer: actionVisualizerContainer
        externalButtonContainer: modernVideoScreen.state === "fullscreen"
            ? fullscreenControlsOverlay.actionButtonContainer
            : actionButtonContainer

        available: controller.playingLive
        onUnavailableAction: actionBanner.trigger()
        onAvailableChanged: actionBanner.reset()

        // A single action is shown as a button of its own, but there is no place for such a button
        // in the fullscreen controls and in the narrow navigation bar, where the overflow menu
        // opens this sheet instead: it has to list the action even when there is just one.
        Binding on externalMode
        {
            when: modernVideoScreen.state === "fullscreen"
                || navigationBarContent.overflow
            value: false
        }

        BannerSource
        {
            id: actionBanner

            text: qsTr("Go to Live to activate soft triggers")
            type: Banner.Warning
            closeable: true
        }
    }

    MediaDownloadBackend
    {
        id: mediaDownloadBackend
        resource: controller.resource
        // The screen owns the dialog: errors may arrive while the screen is being destroyed.
        onErrorOccurred: Workflow.openStandardPopup(title, description, modernVideoScreen)
    }

    DownloadMediaDurationSheet
    {
        id: downloadMediaSheet

        preferredEdge: StyleHints.preferredSheetEdge

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

                onClicked: LayoutController.enterFullscreen(
                    d.verticalVideo ? Qt.PortraitOrientation : Qt.LandscapeOrientation)

                // To block camera swipe if the button is dragged.
                DragHandler { target: null }
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
                    state: controller.dummyState

                    onLogInClicked:
                        controller.resourceHelper.cloudAuthorize()
                }
            }
        }

        Ptz
        {
            id: ptz

            targetVideo: video
            resource: controller.resource
            customRotation: controller.resourceHelper.customRotation

            active: d.ptzMode
            overlayStyle: LayoutController.fullscreen
            opacity: d.controlsOpacity

            onActiveChanged:
            {
                if (active)
                    video.to1xScale()
            }

            onMoveOnTapModeChanged:
            {
                if (moveOnTapMode)
                    video.fitToBounds()
            }

            onClosed: d.mode = VideoScreenUtils.VideoScreenMode.Navigation
        }

        CustomPopupDimmer
        {
            id: ptzSheetDimmer

            popup: ptz.sheet
            parent: modernVideoScreen.contentItem
            active: opened && !LayoutController.fullscreen

            anchors.top: (parent === modernVideoScreen.contentItem) ? navigationBar.top : undefined
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
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

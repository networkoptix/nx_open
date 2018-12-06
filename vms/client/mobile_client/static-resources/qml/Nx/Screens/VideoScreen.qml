import QtQuick 2.6

import Nx 1.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Models 1.0
import Nx.Settings 1.0
import com.networkoptix.qml 1.0

import "private/VideoScreen"
import "private/VideoScreen/utils.js" as VideoScreenUtils
import "private/VideoScreen/Ptz"

PageBase
{
    id: videoScreen
    objectName: "videoScreen"

    property alias resourceId: videoScreenController.resourceId
    property alias initialScreenshot: screenshot.source
    property QnCameraListModel camerasModel: null
    property real targetTimestamp: -1

    backgroundColor: "black"

    VideoScreenController
    {
        id: videoScreenController

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
    }

    Object
    {
        id: d

        property alias controller: videoScreenController

        property bool animatePlaybackControls: true
        property bool showOfflineStatus: false
        property bool showNoLicensesWarning:
            videoScreenController.noLicenses
            && !videoScreenController.mediaPlayer.liveMode
        property bool showDefaultPasswordWarning:
            videoScreenController.hasDefaultCameraPassword
            && videoScreenController.mediaPlayer.liveMode

        property bool cameraWarningVisible:
            ((showOfflineStatus
                || videoScreenController.cameraOffline
                || videoScreenController.cameraUnauthorized
                || videoScreenController.failed
                || videoScreenController.noVideoStreams)
                && !videoScreenController.mediaPlayer.playing)
            || videoScreenController.hasOldFirmware
            || videoScreenController.tooManyConnections
            || videoScreenController.ioModuleWarning
            || videoScreenController.ioModuleAudioPlaying
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
                if (videoScreenController.serverOffline)
                {
                    exitFullscreen()
                    controlsVisible = false
                    uiVisible = true
                }
                else if (videoScreenController.cameraOffline)
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

    header: ToolBar
    {
        id: toolBar

        y: statusBarHeight
        title: videoScreenController.resourceHelper.resourceName
        leftButtonIcon.source: lp("/images/arrow_back.png")
        onLeftButtonClicked: Workflow.popCurrentScreen()
        background: Image
        {            
            y: -toolBar.statusBarHeight
            x: -mainWindow.leftPadding
            width: mainWindow.width
            height: 96
            source: lp("/images/toolbar_gradient.png")
        }

        opacity: d.uiOpacity
        visible: opacity > 0
        titleOpacity: d.cameraUiOpacity

        controls:
        [
            MotionAreaButton
            {
                visible: video.motionController && video.motionController.customRoiExists
                anchors.verticalCenter: parent.verticalCenter

                text: qsTr("Area")
                icon.source: lp("/images/close.png")

                onClicked:
                {
                    if (video.motionController)
                        video.motionController.clearCustomRoi()
                }
            },

            IconButton
            {
                icon.source: lp("/images/more_vert.png")
                onClicked:
                {
                    menu.open()
                }
            }
        ]
    }

    Menu
    {
        id: menu

        parent: videoScreen
        x: parent.width - width - 8
        y: header.y + 8

        MenuItem
        {
            text: qsTr("Change Quality")
            onClicked:
            {
                var customQualities = [ 1080, 720, 480, 360 ]
                var allVideoQualities =
                    [ MediaPlayer.LowVideoQuality, MediaPlayer.HighVideoQuality ]
                        .concat(customQualities)

                var player = videoScreenController.mediaPlayer

                var dialog = Workflow.openDialog(
                    "Screens/private/VideoScreen/QualityDialog.qml",
                    {
                        "actualQuality": player.currentResolution,
                        "activeQuality": player.videoQuality,
                        "customQualities": customQualities,
                        "availableVideoQualities":
                            player.availableVideoQualities(allVideoQualities),
                        "transcodingSupportStatus":
                            player.transcodingStatus()
                    }
                )

                dialog.onActiveQualityChanged.connect(
                    function()
                    {
                        settings.lastUsedQuality = dialog.activeQuality
                    }
                )
            }
        }
        MenuItem
        {
            text: qsTr("Information")
            checkable: true
            checked: showCameraInfo
            onCheckedChanged: showCameraInfo = checked
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

    Loader
    {
        id: video

        property Item motionController: sourceComponent == scalableVideoComponent
            ? item.motionSearchController
            : null

        y: toolBar.visible ? -header.height : 0
        x: -mainWindow.leftPadding
        width: mainWindow.width
        height: mainWindow.height

        visible: dummyLoader.status != Loader.Ready && !screenshot.visible
        opacity: d.cameraUiOpacity

        sourceComponent:
            videoScreenController.resourceHelper.fisheyeParams.enabled
                ? fisheyeVideoComponent
                : scalableVideoComponent

        function clear()
        {
            if (item)
                item.clear()
        }
    }

    Component
    {
        id: scalableVideoComponent

        ScalableVideo
        {
            mediaPlayer: videoScreenController.mediaPlayer
            resourceHelper: videoScreenController.resourceHelper
            videoCenterHeightOffsetFactor: 1 / 3
            onClicked: toggleUi()
            motionSearchController.cameraRotation: videoScreenController.resourceHelper.customRotation
            motionSearchController.motionProvider.mediaPlayer: videoScreenController.mediaPlayer
        }
    }

    Component
    {
        id: fisheyeVideoComponent
        FisheyeVideo
        {
            mediaPlayer: videoScreenController.mediaPlayer
            resourceHelper: videoScreenController.resourceHelper
            videoCenterHeightOffsetFactor: 1 / 3
            onClicked: toggleUi()
        }
    }

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
            var isFisheye = videoScreenController.resourceHelper.fisheyeParams.enabled
            var windowSize = Qt.size(mainWindow.width, mainWindow.height)
            return isFisheye
                ? fillBounds(sourceSize, windowSize)
                : fitToBounds(sourceSize, windowSize)
        }

        width: boundingSize.width
        height: boundingSize.height

        y: (mainWindow.height - height) / 3 - header.height
        x: (mainWindow.width - width) / 2 - mainWindow.leftPadding
        visible: status == Image.Ready && !dummyLoader.visible
        opacity: d.cameraUiOpacity
    }

    Item
    {
        id: content

        width: mainWindow.availableWidth
        height: mainWindow.availableHeight - header.height - toolBar.statusBarHeight
        y: toolBar.statusBarHeight

        Loader
        {
            id: informationLabelLoader
            anchors.right: parent.right
            anchors.rightMargin: 8
            opacity: Math.min(d.uiOpacity, d.cameraUiOpacity)
            active: showCameraInfo
            sourceComponent: InformationLabel
            {
                videoScreenController: d.controller
            }
        }

        Loader
        {
            id: dummyLoader

            readonly property bool needOffset: item && item.onlyCompactTitleIsVisible

            y: needOffset ? -header.height : 0
            x: -mainWindow.leftPadding
            width: mainWindow.width
            height: mainWindow.height - toolBar.statusBarHeight - (needOffset ? 0 : header.height)

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
                    state: videoScreenController.dummyState

                    MouseArea
                    {
                        anchors.fill: parent
                        onClicked: toggleUi()
                    }
                }
            }
        }

        Image
        {
            id: controlsShadowGradient

            property real customHeight:
            {
                if (d.mode == VideoScreenUtils.VideoScreenMode.Ptz || ptzPanel.moveOnTapMode)
                    return getNavigationBarHeight()

                return videoNavigation.buttonsPanelHeight + getNavigationBarHeight()
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
                var ptzMode = d.mode == VideoScreenUtils.VideoScreenMode.Ptz
                var visiblePtzControls = ptzMode && d.uiVisible
                if (visiblePtzControls || ptzPanel.moveOnTapMode)
                    return 1

                return ptzMode ? d.uiOpacity : videoNavigation.opacity
            }
        }

        PtzPanel
        {
            id: ptzPanel

            preloaders.parent: video.item
            preloaders.height: video.item.fitSize ? video.item.fitSize.height : 0
            preloaders.x: (video.item.width - preloaders.width) / 2
            preloaders.y: (video.item.height - preloaders.height) / 3

            width: parent.width
            anchors.bottom: parent.bottom

            controller.resourceId: videoScreenController.resourceHelper.resourceId
            customRotation: videoScreenController.resourceHelper.customRotation

            opacity: Math.min(d.uiOpacity, d.controlsOpacity)
            visible: opacity > 0 && d.mode === VideoScreenUtils.VideoScreenMode.Ptz

            onCloseButtonClicked: d.mode = VideoScreenUtils.VideoScreenMode.Navigation

            onMoveOnTapModeChanged:
            {
                if (ptzPanel.moveOnTapMode)
                {
                    hideUi()
                    moveOnTapOverlay.open()
                    video.item.fitToBounds()

                    // Workaround. Overwise it moves content to wrong place.
                    // TODO: investigate and get rid of this workaround
                    video.item.fitToBounds()
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
                if (videoScreenController.resourceHelper.fisheyeParams.enabled || !video.item)
                    return

                var mapped = contentItem.mapToItem(video.item, pos.x, pos.y)
                var data = video.item.getMoveViewportData(mapped)
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
            id: videoNavigation

            changingMotionRoi:
            {
                if (!video.motionController)
                    return false;

                if (video.motionController.drawingRoi)
                    return false;

                return video.item.moving && !video.motionController.customRoiExists;
            }

            canViewArchive: videoScreenController.accessRightsHelper.canViewArchive
            animatePlaybackControls: d.animatePlaybackControls
            videoScreenController: d.controller
            controlsOpacity: d.cameraUiOpacity
            onPtzButtonClicked:
            {
                d.mode = VideoScreenUtils.VideoScreenMode.Ptz
                video.item.to1xScale()
            }

            anchors.bottom: parent.bottom
            width: parent.width

            visible: opacity > 0 && d.mode === VideoScreenUtils.VideoScreenMode.Navigation
            opacity: Math.min(d.uiOpacity, d.controlsOpacity)
            onSwitchToPreviousCamera:
            {
                if (!camerasModel)
                    return

                switchToCamera(
                    camerasModel.previousResourceId(videoScreen.resourceId)
                        || camerasModel.previousResourceId(""))
            }

            onSwitchToNextCamera:
            {
                if (!camerasModel)
                    return

                switchToCamera(
                    camerasModel.nextResourceId(videoScreen.resourceId)
                        || camerasModel.nextResourceId(""))
            }

            motionFilter: video.sourceComponent == scalableVideoComponent
                ? video.item.motionSearchController.motionFilter
                : ""

            motionSearchMode:
                video.sourceComponent == scalableVideoComponent
                && video.item.motionSearchController.motionSearchMode

            onMotionSearchModeChanged:
            {
                if (video.sourceComponent == scalableVideoComponent)
                    video.item.motionSearchController.motionSearchMode = motionSearchMode
            }
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

        color: ColorTheme.base3
        visible: mainWindow.hasNavigationBar
        width: mainWindow.width - parent.width
        height: video.height
        x: mainWindow.leftPadding ? -mainWindow.leftPadding : parent.width
        anchors.top: video.top
        opacity: Math.min(videoNavigation.opacity, d.cameraUiOpacity)
    }

    SequentialAnimation
    {
        id: cameraSwitchAnimation

        property string newResourceId
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
                videoScreen.resourceId = cameraSwitchAnimation.newResourceId
                initialScreenshot = cameraSwitchAnimation.thumbnail
                video.clear()
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
        if (activePage)
        {
            videoScreenController.start(targetTimestamp)
            targetTimestamp = -1
        }
    }

    Component.onDestruction: exitFullscreen()

    function hideUi()
    {
        d.uiVisible = false
        if (Utils.isMobile())
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

    function switchToCamera(id)
    {
        videoNavigation.motionSearchMode = false
        cameraSwitchAnimation.stop()
        cameraSwitchAnimation.newResourceId = id
        if (videoScreenController.mediaPlayer.liveMode)
        {
            cameraSwitchAnimation.thumbnail = camerasModelAccessor.getData(
                camerasModel.rowByResourceId(id), "thumbnail")
        }
        else
        {
            cameraSwitchAnimation.thumbnail = ""
        }

        cameraSwitchAnimation.start()
    }
}

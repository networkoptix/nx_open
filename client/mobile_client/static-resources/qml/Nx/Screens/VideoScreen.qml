import QtQuick 2.6
import Nx 1.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Models 1.0
import Nx.Settings 1.0
import com.networkoptix.qml 1.0

import "private/VideoScreen"

PageBase
{
    id: videoScreen
    objectName: "videoScreen"

    property alias resourceId: videoScreenController.resourceId
    property alias initialScreenshot: screenshot.source
    property QnCameraListModel camerasModel: null

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

        property var videoNavigation: navigationLoader.item

        property bool showOfflineStatus: false
        property bool cameraWarningVisible:
            (showOfflineStatus
                || videoScreenController.cameraOffline
                || videoScreenController.cameraUnauthorized
                || videoScreenController.failed)
            && !videoScreenController.mediaPlayer.playing

        readonly property bool applicationActive: Qt.application.state === Qt.ApplicationActive

        property real uiOpacity: 1.0
        Behavior on uiOpacity
        {
            NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
        }

        property real navigationOpacity: 1.0
        Behavior on navigationOpacity
        {
            NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
        }

        property real cameraUiOpacity: 1.0

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
                    navigationOpacity = 0.0
                    uiOpacity = 1.0
                }
                else if (videoScreenController.cameraOffline)
                {
                    showUi()
                }
            }
            else
            {
                d.navigationOpacity = 1.0
            }
        }

        onApplicationActiveChanged:
        {
            if (!applicationActive)
                showUi()
        }
    }

    sideNavigationEnabled: false

    header: ToolBar
    {
        id: toolBar

        y: statusBarHeight
        title: videoScreenController.resourceHelper.resourceName
        leftButtonIcon: lp("/images/arrow_back.png")
        onLeftButtonClicked: Workflow.popCurrentScreen()
        background: Image
        {
            anchors.fill: parent
            anchors.topMargin: -toolBar.statusBarHeight
            source: lp("/images/toolbar_gradient.png")
        }

        opacity: d.uiOpacity
        titleOpacity: d.cameraUiOpacity

        controls:
        [
            IconButton
            {
                icon: lp("/images/more_vert.png")
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
                var dialog = Workflow.openDialog(
                    "Screens/private/VideoScreen/QualityDialog.qml",
                    {
                        "actualQuality": videoScreenController.mediaPlayer.currentResolution,
                        "activeQuality": videoScreenController.mediaPlayer.videoQuality
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

    Loader
    {
        id: video

        y: -header.height
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
        width: parent.width
        height: width * sourceSize.height / sourceSize.width
        y: (mainWindow.height - height) / 3 - header.height
        visible: status == Image.Ready
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
            anchors.fill: parent
            visible: active
            active: d.cameraWarningVisible

            sourceComponent: Component
            {
                VideoDummy
                {
                    y: -header.height
                    width: mainWindow.width
                    height: mainWindow.height
                    state: videoScreenController.dummyState

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

            x: 8
            width: parent.width - x * 2
            anchors.bottom: parent.bottom

            controller.resourceId: videoScreenController.resourceHelper.resourceId
        }

        Loader
        {
            id: navigationLoader

            anchors.bottom: parent.bottom
            width: parent.width

            visible: opacity > 0 && !ptzPanel.visible
            opacity: Math.min(d.uiOpacity, d.navigationOpacity)

            sourceComponent:
                videoScreenController.accessRightsHelper.canViewArchive
                    ? navigationComponent
                    : liveNavigationComponent

            Button
            {
                anchors.verticalCenter: parent.bottom
                anchors.verticalCenterOffset: -150 - 64
                padding: 8
                width: 56
                height: width
                color: ColorTheme.transparent(ColorTheme.base5, 0.2)
                icon: lp("/images/previous.png")
                radius: width / 2
                z: 1
                onClicked: switchToPreviousCamera()
            }

            Button
            {
                anchors.verticalCenter: parent.bottom
                anchors.verticalCenterOffset: -150 - 64
                anchors.right: parent.right
                padding: 8
                width: 56
                height: width
                color: ColorTheme.transparent(ColorTheme.base5, 0.2)
                icon: lp("/images/next.png")
                radius: width / 2
                z: 1
                onClicked: switchToNextCamera()
            }

            Component
            {
                id: navigationComponent

                VideoNavigation
                {
                    videoScreenController: d.controller
                    controlsOpacity: d.cameraUiOpacity
                    ptzAvailable: ptzPanel.controller.available
                        && videoScreenController.accessRightsHelper.canManagePtz
                        && !videoScreenController.offline
                    onPtzButtonClicked: ptzPanel.visible = true
                }
            }

            Component
            {
                id: liveNavigationComponent

                LiveVideoNavigation
                {
                    videoScreenController: d.controller
                }
            }
        }

    }

    Rectangle
    {
        id: navigationBarTint

        color: ColorTheme.base3
        width: mainWindow.width - parent.width
        height: video.height
        anchors.left: parent.right
        anchors.top: video.top
        opacity: Math.min(navigationLoader.opacity, d.cameraUiOpacity)
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
                videoScreen.resourceId = cameraSwitchAnimation.newResourceId
                initialScreenshot = cameraSwitchAnimation.thumbnail
                video.clear()
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

    Component.onDestruction: exitFullscreen()

    function hideUi()
    {
        d.uiOpacity = 0.0
        if (Utils.isMobile())
            enterFullscreen()
    }

    function showUi()
    {
        exitFullscreen()
        d.uiOpacity = 1.0
    }

    function toggleUi()
    {
        if (navigationLoader.visible)
            hideUi()
        else
            showUi()
    }

    function switchToCamera(id)
    {
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

    function switchToPreviousCamera()
    {
        if (!camerasModel)
            return

        switchToCamera(
            camerasModel.previousResourceId(videoScreen.resourceId)
                || camerasModel.previousResourceId(""))
    }

    function switchToNextCamera()
    {
        if (!camerasModel)
            return

        switchToCamera(
            camerasModel.nextResourceId(videoScreen.resourceId)
                || camerasModel.nextResourceId(""))
    }
}

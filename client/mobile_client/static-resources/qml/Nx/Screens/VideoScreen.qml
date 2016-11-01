import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private/VideoScreen"

PageBase
{
    id: videoScreen
    objectName: "videoScreen"

    property alias resourceId: videoScreenController.resourceId
    property alias initialScreenshot: screenshot.source

    VideoScreenController
    {
        id: videoScreenController

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

        property var videoNavigation: navigationLoader.item

        property bool showOfflineStatus: false
        property bool cameraWarningVisible:
            (showOfflineStatus
                || videoScreenController.cameraOffline
                || videoScreenController.cameraUnauthorized
                || videoScreenController.failed)
            && !videoScreenController.mediaPlayer.playing

        Timer
        {
            id: offlineStatusDelay
            interval: 2000
            onTriggered: d.showOfflineStatus = true
        }

        onShowOfflineStatusChanged:
        {
            if (cameraWarningVisible)
            {
                if (videoScreenController.serverOffline)
                {
                    exitFullscreen()
                    navigationLoader.opacity = 0.0
                    navigationBarTint.opacity = 0.0
                    toolBar.opacity = 1.0
                }
                else if (videoScreenController.cameraOffline)
                {
                    showUi()
                }
            }
            else
            {
                navigationLoader.opacity = 1.0
                navigationBarTint.opacity = 1.0
            }
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

        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

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
                        videoScreenController.mediaPlayer.videoQuality = dialog.activeQuality
                    }
                )
            }
        }
        MenuItem
        {
            text: qsTr("Information")
        }
    }

    ScalableVideo
    {
        id: video

        y: -header.height
        width: mainWindow.width
        height: mainWindow.height

        visible: dummyLoader.status != Loader.Ready && !screenshot.visible

        source: videoScreenController.mediaPlayer
        customAspectRatio: (videoScreenController.resourceHelper.customAspectRatio
            || videoScreenController.mediaPlayer.aspectRatio)
        videoRotation: videoScreenController.resourceHelper.customRotation

        onClicked: toggleUi()
    }

    Image
    {
        id: screenshot
        width: parent.width
        height: sourceSize.height == 0 ? 0 : width * sourceSize.height / sourceSize.width
        y: (mainWindow.height - height) / 3 - header.height
        visible: status == Image.Ready
    }

    Loader
    {
        id: dummyLoader
        anchors.fill: parent
        visible: active
        sourceComponent: dummyComponent
        active: d.cameraWarningVisible
    }

    Component
    {
        id: dummyComponent

        VideoDummy
        {
            width: videoScreen.width
            state: videoScreenController.dummyState
        }
    }

    MouseArea
    {
        enabled: dummyLoader.visible
        anchors.fill: parent
        onClicked: toggleUi()
    }

    Loader
    {
        id: navigationLoader

        anchors.bottom: parent.bottom
        width: parent.width

        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        sourceComponent: (videoScreenController.accessRightsHelper.canViewArchive
            ? navigationComponent : liveNavigationComponent)
    }

    Component
    {
        id: navigationComponent

        VideoNavigation
        {
            mediaPlayer: videoScreenController.mediaPlayer
        }
    }

    Component
    {
        id: liveNavigationComponent

        LiveVideoNavigation
        {
            mediaPlayer: videoScreenController.mediaPlayer
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

        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
    }


    function hideUi()
    {
        navigationLoader.opacity = 0.0
        toolBar.opacity = 0.0
        navigationBarTint.opacity = 0.0
        if (Utils.isMobile())
            enterFullscreen()
    }

    function showUi()
    {
        exitFullscreen()
        navigationLoader.opacity = 1.0
        toolBar.opacity = 1.0
        navigationBarTint.opacity = 1.0
    }

    function toggleUi()
    {
        if (navigationLoader.visible)
            hideUi()
        else
            showUi()
    }
}

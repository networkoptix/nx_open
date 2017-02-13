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
    property QnCameraListModel camerasModel: null

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

        property alias controller: videoScreenController

        property var videoNavigation: navigationLoader.item

        property bool showOfflineStatus: false
        property bool cameraWarningVisible:
            (showOfflineStatus
                || videoScreenController.cameraOffline
                || videoScreenController.cameraUnauthorized
                || videoScreenController.failed)
            && !videoScreenController.mediaPlayer.playing

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
            checkable: true
            checked: showCameraInfo
            onCheckedChanged: showCameraInfo = checked
        }
    }

    ScalableVideo
    {
        id: video

        y: -header.height
        width: mainWindow.width
        height: mainWindow.height

        visible: dummyLoader.status != Loader.Ready && !screenshot.visible

        mediaPlayer: videoScreenController.mediaPlayer
        resourceHelper: videoScreenController.resourceHelper

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
        id: informationLabelLoader
        anchors.right: parent.right
        anchors.rightMargin: 8
        y: header.y
        opacity: d.uiOpacity
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

            MouseArea
            {
                anchors.fill: parent
                onClicked: toggleUi()
            }
        }
    }

    Loader
    {
        id: navigationLoader

        anchors.bottom: parent.bottom
        width: parent.width

        visible: opacity > 0
        opacity: Math.min(d.uiOpacity, d.navigationOpacity)

        sourceComponent: (videoScreenController.accessRightsHelper.canViewArchive
            ? navigationComponent : liveNavigationComponent)

        Button
        {
            anchors.verticalCenter: parent.bottom
            anchors.verticalCenterOffset: -150 - 64
            x: 8
            padding: 0
            leftPadding: 0
            rightPadding: 0
            width: 40
            height: width
            color: ColorTheme.transparent(ColorTheme.base5, 0.2)
            icon: lp("/images/previous.png")
            radius: width / 2
            z: 1
            onClicked:
            {
                if (!camerasModel)
                    return

                videoScreen.resourceId = camerasModel.previousResourceId(videoScreen.resourceId)
                    || camerasModel.previousResourceId("")
            }
        }

        Button
        {
            anchors.verticalCenter: parent.bottom
            anchors.verticalCenterOffset: -150 - 64
            anchors.right: parent.right
            anchors.rightMargin: 8
            padding: 0
            leftPadding: 0
            rightPadding: 0
            width: 40
            height: width
            color: ColorTheme.transparent(ColorTheme.base5, 0.2)
            icon: lp("/images/next.png")
            radius: width / 2
            z: 1
            onClicked:
            {
                if (!camerasModel)
                    return

                videoScreen.resourceId = camerasModel.nextResourceId(videoScreen.resourceId)
                    || camerasModel.nextResourceId("")
            }
        }
    }

    Component
    {
        id: navigationComponent

        VideoNavigation
        {
            videoScreenController: d.controller
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

    Rectangle
    {
        id: navigationBarTint

        color: ColorTheme.base3
        width: mainWindow.width - parent.width
        height: video.height
        anchors.left: parent.right
        anchors.top: video.top
        opacity: navigationLoader.opacity
    }


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
}

import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private/VideoScreen"

PageBase
{
    id: videoScreen

    property string resourceId
    property string initialScreenshot

    QnMediaResourceHelper
    {
        id: resourceHelper
        resourceId: videoScreen.resourceId
    }

    QnCameraAccessRightsHelper
    {
        id: accessRightsHelper
        resourceId: videoScreen.resourceId
    }

    MediaPlayer
    {
        id: player

        resourceId: videoScreen.resourceId

        onPlayingChanged:
        {
            if (playing)
                video.screenshotSource = ""

            setKeepScreenOn(playing)
        }

        maxTextureSize: getMaxTextureSize()
    }

    Object
    {
        id: d

        property var videoNavigation: navigationLoader.item
        readonly property bool serverOffline: connectionManager.connectionState == QnConnectionManager.Connecting ||
                                              connectionManager.connectionState == QnConnectionManager.Disconnected
        readonly property bool cameraOffline: player.liveMode && resourceHelper.resourceStatus == QnMediaResourceHelper.Offline
        readonly property bool cameraUnauthorized: player.liveMode && resourceHelper.resourceStatus == QnMediaResourceHelper.Unauthorized
        readonly property bool failed: player.failed
        readonly property bool offline: serverOffline || cameraOffline

        property bool showOfflineStatus: false
        property bool cameraWarningVisible: (showOfflineStatus || cameraUnauthorized || d.failed) && !player.playing

        property bool resumeOnActivate: false
        property bool resumeAtLive: false

        onOfflineChanged:
        {
            if (offline)
            {
                offlineStatusDelay.restart()
            }
            else
            {
                offlineStatusDelay.stop()
                showOfflineStatus = false
            }
        }

        Timer
        {
            id: offlineStatusDelay

            interval: 20 * 1000
            repeat: false
            running: false

            onTriggered: d.showOfflineStatus = true
        }

        onShowOfflineStatusChanged: updateOfflineDisplay()

        onFailedChanged:
        {
            if (failed)
                player.stop()
        }

        function updateOfflineDisplay()
        {
            if (cameraWarningVisible)
            {
                if (serverOffline)
                {
                    exitFullscreen()
                    navigationLoader.opacity = 0.0
                    navigationBarTint.opacity = 0.0
                    toolBar.opacity = 1.0
                }
                else if (cameraOffline)
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

        Connections
        {
            target: Qt.application
            onStateChanged:
            {
                if (!Utils.isMobile())
                    return

                if (Qt.application.state != Qt.ApplicationActive)
                {
                    d.resumeOnActivate = player.playing
                    player.pause()
                    showUi()
                }
                else if (Qt.application.state == Qt.ApplicationActive)
                {
                    if (d.resumeOnActivate)
                        player.play()
                }
            }
        }
    }

    header: ToolBar
    {
        id: toolBar

        title: resourceHelper.resourceName
        leftButtonIcon: lp("/images/arrow_back.png")
        onLeftButtonClicked: Workflow.popCurrentScreen()
        background: Image
        {
            anchors.fill: parent
            anchors.topMargin: -toolBar.statusBarHeight
            source: lp("/images/toolbar_gradient.png")
        }

        opacity: liteMode ? 0.0 : 1.0
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
        y: 8

        MenuItem
        {
            text: qsTr("Change Quality")
            onClicked:
            {
                var dialog = Workflow.openDialog("Screens/private/VideoScreen/QualityDialog.qml")
                dialog.actualQuality = player.currentResolution
                dialog.activeQuality = player.videoQuality
                dialog.onActiveQualityChanged.connect(
                    function()
                    {
                        player.videoQuality = dialog.activeQuality
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

        visible: dummyLoader.status != Loader.Ready

        source: player
        screenshotSource: initialScreenshot
        aspectRatio: screenshotSource == initialScreenshot ? resourceHelper.rotatedAspectRatio : resourceHelper.aspectRatio
        videoRotation: screenshotSource == initialScreenshot ? 0 : resourceHelper.rotation

        onClicked:
        {
            if (navigationLoader.visible)
                hideUi()
            else
                showUi()
        }
    }

    Loader
    {
        id: dummyLoader

        visible: d.cameraWarningVisible

        sourceComponent: visible ? dummyComponent : undefined

        y:
        {
            var minY = (parent.height - height) / 6
            var maxY = navigationLoader.y - height - 56
            var y = (parent.height - height) / 2

            return Math.max(minY, Math.min(maxY, y))
        }
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Component
    {
        id: dummyComponent

        Column
        {
            id: videoDummy

            Image
            {
                width: 136
                height: 136

                anchors.horizontalCenter: parent.horizontalCenter

                source:
                {
                    if (d.serverOffline)
                        return lp("/images/server_offline_1.png")
                    else if (d.cameraUnauthorized)
                        return lp("/images/camera_locked_1.png")
                    else if (d.cameraOffline)
                        return lp("/images/camera_offline_1.png")
                    else if (d.failed)
                        return lp("/images/camera_warning_1.png")
                    else
                        return ""
                }
            }

            Text
            {
                height: 96
                color: ColorTheme.base13

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 32
                font.weight: Font.Normal

                wrapMode: Text.WordWrap
                width: videoScreen.width

                anchors.horizontalCenter: parent.horizontalCenter

                text:
                {
                    if (d.serverOffline)
                        return qsTr("Server offline")
                    else if (d.cameraUnauthorized)
                        return qsTr("Authentication\nrequired")
                    else if (d.cameraOffline)
                        return qsTr("Camera offline")
                    else if (d.failed)
                        return qsTr("Can't load video")
                    else
                        return ""
                }
            }
        }
    }

    Loader
    {
        id: navigationLoader

        anchors.bottom: parent.bottom
        width: parent.width

        visible: opacity > 0
        opacity: liteMode ? 0.0 : 1.0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        Component.onCompleted:
        {
            if (!liteMode)
            {
                sourceComponent = accessRightsHelper.canViewArchive
                    ? navigationComponent : liveNavigationComponent
            }
        }
    }

    Component
    {
        id: navigationComponent

        VideoNavigation
        {
            mediaPlayer: player
        }
    }

    Component
    {
        id: liveNavigationComponent

        LiveVideoNavigation
        {
            mediaPlayer: player
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
        opacity: liteMode ? 0.0 : 1.0

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

    Keys.onReturnPressed:
    {
        if (!liteMode)
            return

        Workflow.popCurrentScreen()
    }

    Keys.onSpacePressed:
    {
        if (navigationLoader.visible)
            hideUi()
        else
            showUi()
    }

    Component.onCompleted:
    {
        player.playLive()
        if (liteMode)
            hideUi()
    }

    Component.onDestruction:
    {
        setKeepScreenOn(false)
    }
}

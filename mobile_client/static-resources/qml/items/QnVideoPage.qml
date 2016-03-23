import QtQuick 2.2
import QtQuick.Window 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"
import ".."

QnPage {
    id: videoPage

    title: mainWindow.currentSystemName

    property string resourceId
    property string initialScreenshot

    QnObject {
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

        Timer {
            id: offlineStatusDelay

            interval: 20 * 1000
            repeat: false
            running: false

            onTriggered: d.showOfflineStatus = true
        }

        onShowOfflineStatusChanged: updateOfflineDisplay()

        onFailedChanged: {
            if (failed)
                videoNavigation.paused = true
        }

        function updateOfflineDisplay() {
            if (cameraWarningVisible) {
                if (serverOffline) {
                    exitFullscreen()
                    navigationLoader.opacity = 0.0
                    navigationBarTint.opacity = 0.0
                    toolBar.opacity = 1.0
                } else if (cameraOffline) {
                    showUi()
                }
            } else {
                navigationLoader.opacity = 1.0
                navigationBarTint.opacity = 1.0
            }
        }

        Connections {
            target: Qt.application
            onStateChanged: {
                if (!Main.isMobile())
                    return

                if (Qt.application.state != Qt.ApplicationActive) {
                    if (!d.videoNavigation.paused) {
                        d.resumeAtLive = player.liveMode
                        d.resumeOnActivate = true
                        d.videoNavigation.paused = true
                    }
                    showUi()
                } else if (Qt.application.state == Qt.ApplicationActive) {
                    if (d.resumeOnActivate) {
                        if (d.resumeAtLive)
                            resourceHelper.updateUrl()
                        d.videoNavigation.paused = false
                        d.resumeOnActivate = false
                        d.resumeAtLive = false
                    }
                }
            }
        }
    }

    QnMediaResourceHelper {
        id: resourceHelper
        resourceId: player.resourceId
    }

    Rectangle {
        width: mainWindow.width
        height: mainWindow.height
        y: -stackView.y
        color: QnTheme.windowBackground
    }

    QnToolBar {
        id: toolBar

        z: 5.0

        anchors.top: undefined
        anchors.bottom: parent.top
        backgroundOpacity: 0.0

        title: resourceHelper.resourceName

        QnMenuBackButton {
            x: dp(10)
            anchors.verticalCenter: parent.verticalCenter
            progress: 1.0
            onClicked: Main.gotoMainScreen()
        }

        QnIconButton {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: dp(8)
            icon: "image://icon/more_vert.png"
            onClicked: {
                cameraMenu.updateGeometry(this)
                cameraMenu.show()
            }
        }
    }

    QnCameraMenu {
        id: cameraMenu
        currentQuality: resourceHelper.resolution
        onSelectQuality: qualityDialog.show()
    }

    QnQualityDialog {
        id: qualityDialog
        resolutionList: resourceHelper.resolutions
        currentResolution: resourceHelper.resolution
        onQualityPicked: {
            player.pause()
            resourceHelper.resolution = resolution
            player.seek(player.position)
            if (!d.videoNavigation.paused)
                player.play()
        }
        onHidden: videoPage.forceActiveFocus()
    }

    QnScalableVideo {
        id: video

        visible: dummyLoader.status != Loader.Ready

        anchors.fill: parent
        anchors.topMargin: -toolBar.fullHeight
        anchors.bottomMargin: -navigationBarPlaceholder.realHeight
        anchors.rightMargin: -navigationBarPlaceholder.realWidth

        source: player
        screenshotSource: initialResourceScreenshot
        aspectRatio: screenshotSource == initialResourceScreenshot ? resourceHelper.rotatedAspectRatio : resourceHelper.aspectRatio
        videoRotation: screenshotSource == initialResourceScreenshot ? 0 : resourceHelper.rotation

        onClicked: {
            if (navigationLoader.visible)
                hideUi()
            else
                showUi()
        }
    }

    Loader {
        id: dummyLoader

        visible: d.cameraWarningVisible

        sourceComponent: visible ? dummyComponent : undefined

        y: {
            var minY = (parent.height - height) / 6
            var maxY = navigationLoader.y - height - dp(56)
            var y = (parent.height - height) / 2

            return Math.max(minY, Math.min(maxY, y))
        }
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Component {
        id: dummyComponent

        Column {
            id: videoDummy

            Image {
                width: dp(136)
                height: dp(136)

                anchors.horizontalCenter: parent.horizontalCenter

                source: {
                    if (d.serverOffline)
                        return "qrc:///images/server_offline_1.png"
                    else if (d.cameraUnauthorized)
                        return "qrc:///images/camera_locked_1.png"
                    else if (d.cameraOffline)
                        return "qrc:///images/camera_offline_1.png"
                    else if (d.failed)
                        return "qrc:///images/camera_warning_1.png"
                    else
                        return ""
                }
            }

            Text {
                height: dp(96)
                color: QnTheme.cameraStatus

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: sp(32)
                font.weight: Font.Normal

                wrapMode: Text.WordWrap
                width: videoPage.width

                anchors.horizontalCenter: parent.horizontalCenter

                text: {
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

    Rectangle {
        id: navigationBarTint

        color: QnTheme.navigationPanelBackground
        width: navigationBarPlaceholder.realWidth
        height: video.height
        anchors.left: parent.right
        anchors.top: video.top

        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
    }

    QnMediaPlayer {
        id: player
        resourceId: videoPage.resourceId

        onPlayingChanged: {
            if (playing)
                video.screenshotSource = ""
        }

        Component.onCompleted: {
            player.maxTextureSize = getMaxTextureSize()
            player.playLive()
        }
    }

    QnCameraAccessRightsHelper {
        id: accessRightsHelper
        resourceId: videoPage.resourceId
    }

    Loader {
        id: navigationLoader

        anchors.bottom: parent.bottom
        width: parent.width

        visible: opacity > 0
        opacity: 1.0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        Component.onCompleted: {
            if (!liteMode)
            {
                sourceComponent = accessRightsHelper.canViewArchive
                    ? navigationComponent : liveNavigationComponent
            }
            setKeepScreenOn(true)
        }

        Component.onDestruction: setKeepScreenOn(false)
    }

    Component {
        id: navigationComponent

        QnVideoNavigation {
            id: videoNavigation
            mediaPlayer: player

            onPausedChanged: {
                setKeepScreenOn(!paused)

                if (paused) {
                    mediaPlayer.pause()
                    return
                }

                if (d.resumeAtLive)
                    mediaPlayer.playLive()
                else
                    mediaPlayer.play()
            }
        }
    }

    Component {
        id: liveNavigationComponent

        QnLiveVideoNavigation {
            id: liveVideoNavigation
            mediaPlayer: player

            property bool paused: false

            readonly property real timelinePosition: -1
            readonly property bool timelineDragging: false

            onPausedChanged: {
                if (paused)
                    mediaPlayer.pause()
                else
                    mediaPlayer.playLive()
            }
        }
    }

    function hideUi() {
        navigationLoader.opacity = 0.0
        toolBar.opacity = 0.0
        navigationBarTint.opacity = 0.0
        if (Main.isMobile())
            enterFullscreen()
    }

    function showUi() {
        exitFullscreen()
        navigationLoader.opacity = 1.0
        toolBar.opacity = 1.0
        navigationBarTint.opacity = 1.0
    }

    focus: true

    Keys.onReleased:
    {
        if (Main.keyIsBack(event.key))
        {
            if (Main.backPressed())
            {
                event.accepted = true
            }
        }
    }

    Keys.onReturnPressed:
    {
        if (!liteMode)
            return

        Main.backPressed()
    }
}

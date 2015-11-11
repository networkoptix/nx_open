import QtQuick 2.2
import QtQuick.Window 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"

QnPage {
    id: videoPlayer

    title: mainWindow.currentSystemName

    property string resourceId
    property string initialScreenshot

    QtObject {
        id: d
        property var videoNavigation: navigationLoader.item
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

        title: player.resourceName

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
        currentQuality: player.resourceHelper.resolution
        onSelectQuality: qualityDialog.show()
    }

    QnQualityDialog {
        id: qualityDialog
        resolutionList: player.resourceHelper.resolutions
        currentResolution: player.resourceHelper.resolution
        onQualityPicked: {
            player.seek(player.position)
            player.resourceHelper.resolution = resolution
        }
        onHidden: videoPlayer.forceActiveFocus()
    }

    QnActiveCameraThumbnailLoader {
        id: thumbnailLoader
        resourceId: videoPlayer.resourceId
        Component.onCompleted: initialize(parent)
    }

    Binding {
        target: thumbnailLoader
        property: "position"
        value: d.videoNavigation.timelinePosition
        when: d.videoNavigation.timelineDragging && !d.videoNavigation.timelineAtLive
    }

    QnScalableVideo {
        id: video

        anchors.fill: parent
        anchors.topMargin: -toolBar.fullHeight
        anchors.bottomMargin: -navigationBarPlaceholder.realHeight
        anchors.rightMargin: -navigationBarPlaceholder.realWidth

        source: player.mediaPlayer
        screenshotSource: initialResourceScreenshot
        aspectRatio: screenshotSource == initialResourceScreenshot ? player.resourceHelper.rotatedAspectRatio : player.resourceHelper.aspectRatio
        videoRotation: screenshotSource == initialResourceScreenshot ? 0 : player.resourceHelper.rotation

        onClicked: {
            if (navigationLoader.visible)
                hideUi()
            else
                showUi()
        }

        function clearScreenshotSource() {
            screenshotSource = ""
        }

        function bindScreenshotSource() {
            screenshotSource = Qt.binding(function(){ return thumbnailLoader.thumbnailUrl })
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
        resourceId: videoPlayer.resourceId

        onPlayingChanged: {
            if (playing)
                video.clearScreenshotSource()
        }

        onLoadingChanged: {
            if (!loading)
                return

            video.bindScreenshotSource()
            thumbnailLoader.forceLoadThumbnail(player.position)
        }

        onTimelinePositionRequest: {
            if (player.playing) {
                video.clearScreenshotSource()
                return
            }

            video.bindScreenshotSource()
            thumbnailLoader.forceLoadThumbnail(player.position)
        }

        Component.onCompleted: player.playLive()
    }

    QnCameraAccessRightsHelper {
        id: accessRightsHelper
        resourceId: videoPlayer.resourceId
    }

    Loader {
        id: navigationLoader

        anchors.bottom: parent.bottom
        width: parent.width

        visible: opacity > 0
        opacity: 1.0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        Component.onCompleted: {
            sourceComponent = accessRightsHelper.canViewArchive ? navigationComponent : liveNavigationComponent
        }
    }

    Component {
        id: navigationComponent

        QnVideoNavigation {
            id: videoNavigation
            mediaPlayer: player

            onTimelineDraggingChanged: {
                if (timelineDragging)
                    video.bindScreenshotSource()
                else if (player.playing)
                    video.clearScreenshotSource()
            }
        }
    }

    Component {
        id: liveNavigationComponent

        QnLiveVideoNavigation {
            id: liveVideoNavigation

            readonly property real timelinePosition: -1
            readonly property bool timelineDragging: false
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

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            if (Main.backPressed()) {
                event.accepted = true
            }
        }
    }
}

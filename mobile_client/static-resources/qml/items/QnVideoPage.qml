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

        title: mediaPlayer.resourceName

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
        currentQuality: mediaPlayer.resourceHelper.resolution
        onSelectQuality: qualityDialog.show()
    }

    QnQualityDialog {
        id: qualityDialog
        resolutionList: mediaPlayer.resourceHelper.resolutions
        currentResolution: mediaPlayer.resourceHelper.resolution
        onQualityPicked: {
            mediaPlayer.seek(mediaPlayer.position)
            mediaPlayer.resourceHelper.resolution = resolution
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
        value: videoNavigation.timelinePosition
        when: videoNavigation.timelineDragging && !mediaPlayer.atLive
    }

    QnScalableVideo {
        id: video

        anchors.fill: parent
        anchors.topMargin: -toolBar.fullHeight
        anchors.bottomMargin: -navigationBarPlaceholder.realHeight
        anchors.rightMargin: -navigationBarPlaceholder.realWidth

        source: mediaPlayer.mediaPlayer
        screenshotSource: initialResourceScreenshot
        screenshotAspectRatio: mediaPlayer.resourceHelper.aspectRatio

        onClicked: {
            if (videoNavigation.visible)
                hideUi()
            else
                showUi()
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
        id: mediaPlayer
        resourceId: videoPlayer.resourceId

        onPlayingChanged: {
            if (playing)
                video.screenshotSource = ""
        }

        onLoadingChanged: {
            if (!loading)
                return

            video.bindScreenshotSource()
            thumbnailLoader.forceLoadThumbnail(mediaPlayer.position)
        }

        onTimelinePositionRequest: {
            video.bindScreenshotSource()
            thumbnailLoader.forceLoadThumbnail(mediaPlayer.position)
        }
    }

    QnVideoNavigation {
        id: videoNavigation
        mediaPlayer: mediaPlayer

        visible: opacity > 0
        opacity: 1.0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        onTimelineDraggingChanged: {
            if (timelineDragging)
                video.bindScreenshotSource()
        }
    }

    Component.onCompleted: {
        mediaPlayer.playLive()
    }

    function hideUi() {
        videoNavigation.opacity = 0.0
        toolBar.opacity = 0.0
        navigationBarTint.opacity = 0.0
        if (Main.isMobile())
            enterFullscreen()
    }

    function showUi() {
        exitFullscreen()
        videoNavigation.opacity = 1.0
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

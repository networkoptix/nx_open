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

    title: mediaPlayer.resourceName

    property string resourceId

    Connections {
        target: menuBackButton
        onClicked: Main.gotoMainScreen()
    }

    QnIconButton {
        parent: toolBar.contentItem
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: dp(8)
        icon: "image://icon/more_vert.png"
        visible: pageStatus == Stack.Active || pageStatus == Stack.Activating
        onClicked: cameraMenu.open(toolBar.width, dp(4))
    }

    QnCameraMenu {
        id: cameraMenu
        currentQuality: mediaPlayer.resourceHelper.resolution
        onSelectQuality: qualityDialog.open()
    }

    QnQualityDialog {
        id: qualityDialog
        resolutionList: mediaPlayer.resourceHelper.resolutions
        currentResolution: mediaPlayer.resourceHelper.resolution
        onQualityPicked: mediaPlayer.resourceHelper.resolution = resolution
    }

    QnScalableVideo {
        id: video

        anchors.fill: parent
        anchors.topMargin: -toolBar.fullHeight
        anchors.bottomMargin: videoNavigation.videoBottomMargin
        Behavior on anchors.bottomMargin { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        source: mediaPlayer.mediaPlayer

        onClicked: {
            if (videoNavigation.visible)
                hideUi()
            else
                showUi()
        }
    }

    QnMediaPlayer {
        id: mediaPlayer
        resourceId: videoPlayer.resourceId
    }

    QnVideoNavigation {
        id: videoNavigation
        mediaPlayer: mediaPlayer

        visible: opacity > 0
        opacity: 1.0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
    }

    Component.onCompleted: {
        mediaPlayer.playLive()
    }

    function hideUi() {
        videoNavigation.opacity = 0.0
        toolBar.opacity = 0.0
        video.anchors.bottomMargin = Qt.binding(function() { return -navigationBarPlaceholder.height })
        video.anchors.rightMargin = Qt.binding(function() { return -navigationBarPlaceholder.width })
        if (Main.isMobile())
            enterFullscreen()
    }

    function showUi() {
        exitFullscreen()
        videoNavigation.opacity = 1.0
        toolBar.opacity = 1.0
        video.anchors.bottomMargin = Qt.binding(function() { return videoNavigation.videoBottomMargin })
        video.anchors.rightMargin = 0
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

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
        onQualityPicked: mediaPlayer.resourceHelper.resolution = resolution
    }

    QnScalableVideo {
        id: video

        anchors.fill: parent
        anchors.topMargin: -toolBar.fullHeight
        anchors.bottomMargin: -navigationBarPlaceholder.realHeight
        anchors.rightMargin: -navigationBarPlaceholder.realWidth

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
        if (Main.isMobile())
            enterFullscreen()
    }

    function showUi() {
        exitFullscreen()
        videoNavigation.opacity = 1.0
        toolBar.opacity = 1.0
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

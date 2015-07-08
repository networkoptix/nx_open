import QtQuick 2.2
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

    VideoOutput {
        id: video

        width: parent.width
        height: parent.height

        source: mediaPlayer.mediaPlayer
    }

    QnMediaPlayer {
        id: mediaPlayer
        resourceId: videoPlayer.resourceId
    }

    QnVideoNavigation {
        id: videoNavigation
        mediaPlayer: mediaPlayer
    }

    Component.onCompleted: {
        mediaPlayer.playLive()
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

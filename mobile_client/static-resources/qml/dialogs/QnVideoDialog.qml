import QtQuick 2.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0
import Material 0.1

Page {
    id: videoPlayer

    title: resourceHelper.resourceName

    property string resourceId

    property var __currentDate: new Date()

    QnMediaResourceHelper {
        id: resourceHelper
    }

    Rectangle {
        anchors.fill: video
        color: "black"
    }

    Video {
        id: video

        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter

        width: parent.width
        height: width / 4 * 3

        source: resourceHelper.mediaUrl
        autoPlay: true

        onStatusChanged: {
            if (metaData.resolution)
                height = Math.min(parent.height, parent.width / metaData.resolution.width * metaData.resolution.height)
        }
    }

    Label {
        id: bigTimeLabel

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: video.bottom
        anchors.topMargin: units.dp(36)

        fontInfo: {
            "size": 56,
            "font": "light"
        }

        text: __currentDate.toTimeString()
    }

    ActionButton {
        id: playbackControl
        iconName: "av/pause"
        anchors.horizontalCenter: parent.horizontalCenter
        y: (video.height + timeline.y) / 2 - height / 2
        width: units.dp(56)
        height: units.dp(56)
    }

    // timeline dummy
    Rectangle {
        id: timeline

        anchors.bottom: parent.bottom
        width: parent.width
        height: units.dp(92)

        color: theme.accentColor
    }

    Timer {
        running: true
        repeat: true
        interval: 1000

        onTriggered: {
            __currentDate = new Date()
        }
    }

    Component.onCompleted: {
        resourceHelper.resourceId = resourceId
    }
}

import QtQuick 2.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0
import Material 0.1

Page {
    id: videoPlayer

    title: resourceHelper.resourceName

    property alias resourceId: resourceHelper.resourceId
    property var __currentDate: new Date()
    property bool videoZoomed: false
    property real timelineTextMargin: 48
    property real cursorWidth: Units.dp(3)

    QnMediaResourceHelper {
        id: resourceHelper
    }

    Flickable {
        id: flickable

        width: parent.width
        height: timeline.y + timelineTextMargin

        contentWidth: width
        contentHeight: height

        PinchArea {
            width: Math.max(flickable.contentWidth, flickable.width)
            height: Math.max(flickable.contentHeight, flickable.height)

            property real initialWidth
            property real initialHeight
            property real maxScale

            onPinchStarted: {
                initialWidth = flickable.contentWidth
                initialHeight = flickable.contentHeight
                maxScale = video.videoWidth / initialWidth
            }

            onPinchUpdated: {
                var scale = Math.min(maxScale, pinch.scale)
                flickable.contentX += pinch.previousCenter.x - pinch.center.x
                flickable.contentY += pinch.previousCenter.y - pinch.center.y
                flickable.resizeContent(initialWidth * scale, initialHeight * scale, pinch.center)
            }

            onPinchFinished: {
                flickable.returnToBounds()
            }

            Video {
                id: video

                property int videoWidth: metaData.resolution ? metaData.resolution.width : 0
                property int videoHeight: metaData.resolution ? metaData.resolution.height : 0

                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter

                width: flickable.contentWidth
                height: flickable.contentHeight

                source: resourceHelper.mediaUrl
                autoPlay: true
            }

            MouseArea {
                anchors.fill: parent
                onWheel: {
                    var scale = wheel.angleDelta.y / 100
                    if (scale < 0)
                        scale = 1.0 / -scale

                    scale = Math.max(Math.min(scale, video.videoWidth / flickable.contentWidth), flickable.width / flickable.contentWidth)
                    flickable.resizeContent(flickable.contentWidth * scale, flickable.contentHeight * scale, Qt.point(wheel.x, wheel.y))
                    flickable.returnToBounds()
                }
            }
        }

    }

    ActionButton {
        id: playbackControl
        iconName: "av/pause"
        anchors.horizontalCenter: parent.horizontalCenter
        y: (video.height + timeline.y) / 2 - height / 2
        width: Units.dp(56)
        height: Units.dp(56)
    }

    // timeline dummy
    Rectangle {
        id: timeline

        anchors.bottom: parent.bottom
        width: parent.width
        height: Units.dp(92)

        color: theme.accentColor
    }

    Label {
        id: bigTimeLabel

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: timeline.top
            bottomMargin: -timelineTextMargin
        }

        fontInfo: {
            "size": 56,
            "font": "light"
        }

        text: __currentDate.toTimeString()

        Timer {
            running: true
            repeat: true
            interval: 1000

            onTriggered: {
                __currentDate = new Date()
            }
        }
    }

    Rectangle {
        color: "white"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: timeline.bottom
        width: cursorWidth
        height: timeline.height - timelineTextMargin
    }
}

import QtQuick 2.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0
import Material 0.1

import "../items"

Page {
    id: videoPlayer

    title: resourceHelper.resourceName

    property alias resourceId: resourceHelper.resourceId
    property bool videoZoomed: false
    property real cursorTickMargin: Units.dp(10)
    property real timelineTextMargin: timeline.height - timeline.chunkBarHeight
    property real cursorWidth: Units.dp(2)

    readonly property var __locale: Qt.locale()

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

    QnTimeline {
        id: timeline

        anchors.bottom: parent.bottom
        width: parent.width
        height: Units.dp(140)

        textColor: "white"
        chunkColor: "#589900"

        chunkBarHeight: Units.dp(36)
    }

    Rectangle {
        id: timeLabelBackground

        property color baseColor: colorTheme.color("nx_baseBackground")

        anchors.centerIn: timeline
        anchors.verticalCenterOffset: -timeline.chunkBarHeight / 2
        width: timeline.height - timeline.chunkBarHeight
        height: timeLabel.width + Units.dp(64)

        rotation: 90
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(timeLabelBackground.baseColor.r, timeLabelBackground.baseColor.g, timeLabelBackground.baseColor.b, 0) }
            GradientStop { position: 0.2; color: timeLabelBackground.baseColor }
            GradientStop { position: 0.8; color: timeLabelBackground.baseColor }
            GradientStop { position: 1.0; color: Qt.rgba(timeLabelBackground.baseColor.r, timeLabelBackground.baseColor.g, timeLabelBackground.baseColor.b, 0) }
        }
    }

    Text {
        id: dateLabel

        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: timeline.verticalCenter
            verticalCenterOffset: -timeline.chunkBarHeight / 2 - timeLabel.height / 2
        }

        font.pixelSize: Units.dp(20)
        font.weight: Font.Light

        text: timeline.positionDate.toLocaleDateString(__locale, qsTr("d MMMM yyyy"))
        color: "white"
    }

    Text {
        id: timeLabel

        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: timeline.verticalCenter
            verticalCenterOffset: -timeline.chunkBarHeight / 2 + dateLabel.height / 2
        }

        font.pixelSize: Units.dp(48)
        font.weight: Font.Light

        text: timeline.positionDate.toTimeString()
        color: "white"
    }

    QnPlaybackController {
        width: parent.width - height / 3
        height: Units.dp(80)
        anchors.bottom: timeline.top
        anchors.horizontalCenter: parent.horizontalCenter
        tickSize: cursorTickMargin
        lineWidth: Units.dp(2)
        color: colorTheme.color("nx_baseText")
        markersBackground: Qt.darker(color, 100)
        highlightColor: "#2fffffff"
    }

    Rectangle {
        color: "white"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: cursorWidth
        height: timeline.chunkBarHeight + cursorTickMargin
    }
}

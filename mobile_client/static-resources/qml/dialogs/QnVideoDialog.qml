import QtQuick 2.2
import QtQuick.Controls.Styles 1.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0
import Material 0.1

import "../items"

Page {
    id: videoPlayer

    title: resourceHelper.resourceName

    property string resourceId
    property bool videoZoomed: false
    property real cursorTickMargin: Units.dp(10)
    property real timelineTextMargin: timeline.height - timeline.chunkBarHeight
    property real cursorWidth: Units.dp(2)

    readonly property var __locale: Qt.locale()

    property var __chunkEndDate
    property var __nextChunkStartDate
    readonly property bool __playing: video.playbackState == MediaPlayer.PlayingState && video.position > 0

    function alignToChunk(pos) {
        if (!timeline.stickToEnd) {
            var startMs = -1
            var startDate

            if (!isNaN(pos.getTime())) {
                startDate = chunkProvider.closestChunkStartDate(pos, true)
                startMs = chunkProvider.closestChunkStartMs(pos, true)
                __chunkEndDate = chunkProvider.closestChunkEndDate(pos, true)
                __nextChunkStartDate = chunkProvider.nextChunkStartDate(pos, true)
            }

            if (startMs === -1) {
                timeline.stickToEnd = true
            } else {
                if (pos < startDate)
                    pos = startDate

                video.setPositionDate(pos)
                resourceHelper.setDateTime(pos)
                timeline.positionDate = pos
            }
        }

        if (timeline.stickToEnd)
            resourceHelper.setLive()
    }

    QnMediaResourceHelper {
        id: resourceHelper
        resourceId: videoPlayer.resourceId
        screenSize: Qt.size(videoPlayer.width, videoPlayer.height)
    }

    QnCameraChunkProvider {
        id: chunkProvider
        resourceId: videoPlayer.resourceId
    }

    Timer {
        id: chunksUpdateTimer
        interval: 30000
        triggeredOnStart: true
        running: true
        onTriggered: chunkProvider.update()
    }

    Video {
        id: video

        property int videoWidth: metaData.resolution ? metaData.resolution.width : 0
        property int videoHeight: metaData.resolution ? metaData.resolution.height : 0
        property date positionDate
        property int __prevPosition: 0
        property bool __updateTimeline: false

        width: parent.width
        height: parent.height

        source: resourceHelper.mediaUrl
        autoPlay: !playbackController.paused

        onPositionChanged: {
            if (position == 0)
                __prevPosition = 0

            setPositionDate(new Date(positionDate.getTime() + (position - __prevPosition)))

            if (timeline.stickToEnd || timeline.dragging)
                return

            if (__chunkEndDate < positionDate) {
                videoPlayer.alignToChunk(__nextChunkStartDate)
            } else if (__updateTimeline) {
                timeline.positionDate = video.positionDate
                __updateTimeline = false
            }
        }

        function setPositionDate(pos) {
            positionDate = pos
            __prevPosition = position
        }
    }

    Timer {
        interval: 5000
        running: !timeline.stickToEnd && !timeline.dragging && __playing
        onTriggered: video.__updateTimeline = true
    }

    Item {
        id: navigator

        width: parent.width
        height: timeline.height + playbackController.height

        y: parent.height - height
        Behavior on y { NumberAnimation { duration: 200 } }

        MouseArea {
            id: navigatorMouseArea
            anchors.fill: navigator
            drag.target: navigator
            drag.axis: Drag.YAxis
            drag.minimumY: videoPlayer.height - navigator.height - navigationPanel.height
            drag.maximumY: videoPlayer.height - navigator.height
            onReleased: {
                var mid = (drag.minimumY + drag.maximumY) / 2
                if (navigator.y < mid)
                    navigator.y = drag.minimumY
                else
                    navigator.y = drag.maximumY
            }
        }

        QnTimeline {
            id: timeline

            anchors.bottom: parent.bottom
            width: parent.width
            height: Units.dp(140)

            endDate: new Date(Date.now() + 5 * 60 * 1000)
            startDate: new Date(Date.now() - 5 * 60 * 1000)

            textColor: "#484848"
            chunkColor: "#589900"

            stickToEnd: true

            font.pixelSize: Units.dp(18)
            font.weight: Font.Bold

            chunkBarHeight: Units.dp(36)

            chunkProvider: chunkProvider
            startBoundDate: chunkProvider.bottomBound

            autoPlay: !stickToEnd && __playing

            onMoveFinished: videoPlayer.alignToChunk(positionDate)
        }

        Rectangle {
            id: navigationPanel
            width: parent.width
            height: Units.dp(56)
            anchors.top: timeline.bottom
            color: "#0d0d0d"

            Button {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                backgroundColor: parent.color
                text: qsTr("LIVE")
                onClicked: timeline.stickToEnd = true
                opacity: timeline.stickToEnd ? 0.0 : 1.0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }

            IconButton {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                iconName: "action/today"
            }

            IconButton {
                id: zoomOutButton
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -width / 2
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                iconName: "content/remove"
            }

            IconButton {
                id: zoomInButton
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: width / 2
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                iconName: "content/add"
            }

            Timer {
                interval: 100
                repeat: true
                running: zoomInButton.pressed || zoomOutButton.pressed
                triggeredOnStart: true
                onTriggered: {
                    if (zoomInButton.pressed)
                        timeline.zoomIn()
                    else
                        timeline.zoomOut()
                }
            }
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

            opacity: timeline.stickToEnd ? 0.0 : 1.0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }

        Text {
            id: timeLabel

            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: timeline.verticalCenter
                verticalCenterOffset: -timeline.chunkBarHeight / 2 + (timeline.stickToEnd ? 0 : dateLabel.height / 2)
            }

            Behavior on anchors.verticalCenterOffset { NumberAnimation { duration: 200 } }

            font.pixelSize: Units.dp(48)
            font.weight: Font.Light

            text: timeline.stickToEnd ? qsTr("LIVE") : timeline.positionDate.toTimeString()
            color: "white"
        }

        QnPlaybackController {
            id: playbackController

            width: parent.width - height / 3
            height: Units.dp(80)
            anchors.bottom: timeline.top
            anchors.horizontalCenter: parent.horizontalCenter
            tickSize: cursorTickMargin
            lineWidth: Units.dp(2)
            color: colorTheme.color("nx_baseText")
            markersBackground: Qt.darker(color, 100)
            highlightColor: "#2fffffff"
            speedEnabled: false

            onPausedChanged: {
                if (paused)
                    video.pause()
                else
                    video.play()
            }
        }

        Rectangle {
            color: "white"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: cursorWidth
            height: timeline.chunkBarHeight + cursorTickMargin
        }
    }
}

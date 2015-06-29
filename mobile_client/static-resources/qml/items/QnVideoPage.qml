import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"

QnPage {
    id: videoPlayer

    title: resourceHelper.resourceName

    property string resourceId
    property bool videoZoomed: false
    property real cursorTickMargin: dp(10)
    property real timelineTextMargin: timeline.height - timeline.chunkBarHeight
    property real cursorWidth: dp(2)

    readonly property var __locale: Qt.locale()

    property var __chunkEndDate
    property var __nextChunkStartDate

    Connections {
        target: menuBackButton
        onClicked: Main.gotoMainScreen()
    }

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

                resourceHelper.setDateTime(pos)
                video.setPositionDate(pos)
                video.stop()
                if (!playbackController.paused)
                    video.play()
                timeline.positionDate = pos
            }
        }

        if (timeline.stickToEnd) {
            resourceHelper.setLive()
            if (!playbackController.paused)
                video.play()
        }
    }

    QnMediaResourceHelper {
        id: resourceHelper
        screenSize: Qt.size(videoPlayer.width, videoPlayer.height)
        resourceId: videoPlayer.resourceId
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
        property bool __playing

        width: parent.width
        height: parent.height - timeline.height
        source: resourceHelper.mediaUrl

        autoPlay: !playbackController.paused

        onPositionChanged: {
            __playing = (playbackState == MediaPlayer.PlayingState) && (position > 0)

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

        onPlaybackStateChanged: __playing = (playbackState == MediaPlayer.PlayingState) && (position > 0)

        onPaused: timeline.stickToEnd = false
        onPlaying: videoPlayer.alignToChunk(timeline.positionDate)

        function setPositionDate(pos) {
            positionDate = pos
            __prevPosition = position
        }
    }

    Timer {
        interval: 5000
        running: !timeline.stickToEnd && !timeline.dragging && video.__playing
        onTriggered: video.__updateTimeline = true
    }

    Item {
        id: navigator

        width: parent.width
        height: timeline.height + playbackController.height

        y: parent.height - height
        Behavior on y {
            id: navigatorYBehavior
            enabled: false
            NumberAnimation {
                duration: 200
                onStopped: navigatorYBehavior.enabled = false
            }
        }

        MouseArea {
            id: navigatorMouseArea
            anchors.fill: navigator
            drag.target: navigator
            drag.axis: Drag.YAxis
            drag.minimumY: videoPlayer.height - navigator.height - navigationPanel.height
            drag.maximumY: videoPlayer.height - navigator.height
            drag.filterChildren: true
            onReleased: {
                var mid = (drag.minimumY + drag.maximumY) / 2
                navigatorYBehavior.enabled = true
                if (navigator.y < mid)
                    navigator.y = drag.minimumY
                else
                    navigator.y = drag.maximumY
            }

            QnTimeline {
                id: timeline

                anchors.bottom: parent.bottom
                width: parent.width
                height: dp(140)

                endDate: new Date(Date.now() + 5 * 60 * 1000)
                startDate: new Date(Date.now() - 5 * 60 * 1000)

                stickToEnd: true

                chunkBarHeight: dp(36)
                textY: (height - chunkBarHeight) / 2

                chunkProvider: chunkProvider
                startBoundDate: chunkProvider.bottomBound

                autoPlay: !stickToEnd && video.__playing

                onMoveFinished: {
                    if (!playbackController.paused)
                        videoPlayer.alignToChunk(positionDate)
                }

                Image {
                    z: -1
                    width: parent.width
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: timeline.chunkBarHeight
                    sourceSize.height: timeline.height - timeline.chunkBarHeight + playbackController.height / 2
                    source: "qrc:///images/timeline_gradient.png"
                }
            }

            Rectangle {
                id: navigationPanel
                width: parent.width
                height: dp(56)
                anchors.top: timeline.bottom
                color: "#0d0d0d"
                clip: true

                QnButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("LIVE")
                    flat: true
                    iconic: true
                    onClicked: timeline.stickToEnd = true
                    opacity: timeline.stickToEnd ? 0.0 : 1.0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }

                QnIconButton {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "qrc:///images/calendar.png"
                    onClicked: calendarPanel.show()
                }

                QnIconButton {
                    id: zoomOutButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: -width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "qrc:///images/minus.png"
                }

                QnIconButton {
                    id: zoomInButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "qrc:///images/plus.png"
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

//            Rectangle {
//                id: timeLabelBackground

//                property color baseColor: QnTheme.windowBackground

//                anchors.centerIn: timeline
//                anchors.verticalCenterOffset: -timeline.chunkBarHeight / 2
//                width: timeline.height - timeline.chunkBarHeight
//                height: timeLiveLabel.width + dp(64)

//                rotation: 90
//                gradient: Gradient {
//                    GradientStop { position: 0.0; color: QnTheme.transparent(timeLabelBackground.baseColor, 0) }
//                    GradientStop { position: 0.2; color: timeLabelBackground.baseColor }
//                    GradientStop { position: 0.8; color: timeLabelBackground.baseColor }
//                    GradientStop { position: 1.0; color: QnTheme.transparent(timeLabelBackground.baseColor, 0) }
//                }
//            }

            Text {
                id: dateLabel

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: timeline.verticalCenter
                    verticalCenterOffset: -timeline.chunkBarHeight / 2 - timeLiveLabel.height / 2
                }

                font.pixelSize: dp(20)
                font.weight: Font.Normal

                text: timeline.positionDate.toLocaleDateString(__locale, qsTr("d MMMM yyyy"))
                color: "white"

                opacity: timeline.stickToEnd ? 0.0 : 1.0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }

            Item {
                id: timeLiveLabel

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: timeline.verticalCenter
                    verticalCenterOffset: -timeline.chunkBarHeight / 2 + (timeline.stickToEnd ? 0 : dateLabel.height / 2)
                }
                Behavior on anchors.verticalCenterOffset { NumberAnimation { duration: 200 } }

                width: timeLabel.width
                height: timeLabel.height

                QnTimeLabel {
                    id: timeLabel
                    dateTime: timeline.positionDate
                    visible: !timeline.stickToEnd
                }

                Text {
                    id: liveLabel
                    anchors.centerIn: timeLabel
                    font.pixelSize: sp(36)
                    font.weight: Font.Normal
                    color: QnTheme.windowText
                    text: qsTr("LIVE")
                    visible: timeline.stickToEnd
                }
            }

            QnPlaybackController {
                id: playbackController

                width: parent.width - height / 3
                height: dp(80)
                anchors.bottom: timeline.top
                anchors.horizontalCenter: parent.horizontalCenter
                tickSize: cursorTickMargin
                lineWidth: dp(2)
                color: QnTheme.windowText
                markersBackground: Qt.darker(color, 100)
                highlightColor: "#2fffffff"
                speedEnabled: false

                onPausedChanged: {
                    if (paused) {
                        video.pause()
                    } else {
                        alignToChunk(timeline.positionDate)
                        video.play()
                    }
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

    Item {
        id: calendarPanel

        width: parent.width
        height: parent.height
        z: 100.0

        visible: opacity > 0.0
        opacity: 0.0

        Rectangle {
            parent: Main.findRootItem(calendarPanel)
            width: parent.width
            height: calendarContent.y + toolBar.height
            opacity: calendarPanel.opacity
            color: "#60000000"
            z: 10.0

            MouseArea {
                anchors.fill: parent
                onClicked: calendarPanel.hide()
            }
        }

        Rectangle {
            anchors.fill: calendarContent
            color: QnTheme.calendarBackground

            QnSideShadow {
                anchors.fill: parent
                position: Qt.TopEdge
                z: 10
            }

            MouseArea {
                anchors.fill: parent
                /* To block mouse events under calendar */
            }
        }

        Column {
            id: calendarContent
            width: parent.width
            y: parent.height - height

            QnChunkedCalendar {
                id: calendar

                width: parent.width
                chunkProvider: chunkProvider

                onDatePicked: {
                    calendarPanel.hide()
                    if (playbackController.paused)
                        timeline.positionDate = date
                    else
                        alignToChunk(date)
                }

                onVisibleChanged: {
                    if (visible)
                        date = timeline.positionDate
                }
            }

            QnButton {
                anchors.right: parent.right
                text: qsTr("Close")
                flat: true
                onClicked: calendarPanel.hide()
            }
        }

        SequentialAnimation {
            id: showAnimation

            ParallelAnimation {
                NumberAnimation {
                    target: calendarPanel
                    property: "opacity"
                    duration: 150
                    easing.type: Easing.OutCubic
                    to: 1.0
                }

                NumberAnimation {
                    target: calendarContent
                    property: "y"
                    duration: 150
                    easing.type: Easing.OutCubic
                    from: calendarPanel.height - calendarContent.height + dp(56)
                    to: calendarPanel.height - calendarContent.height
                }
            }
        }

        SequentialAnimation {
            id: hideAnimation

            ParallelAnimation {
                NumberAnimation {
                    target: calendarPanel
                    property: "opacity"
                    duration: 150
                    easing.type: Easing.OutCubic
                    to: 0.0
                }

                NumberAnimation {
                    target: calendarContent
                    property: "y"
                    duration: 150
                    easing.type: Easing.OutCubic
                    to: calendarPanel.height - calendarContent.height + dp(56)
                }
            }
        }

        function show() {
            hideAnimation.stop()
            showAnimation.start()
        }

        function hide() {
            showAnimation.stop()
            hideAnimation.start()
        }
    }
}

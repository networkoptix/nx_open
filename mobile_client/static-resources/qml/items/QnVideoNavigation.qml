import QtQuick 2.4
import QtGraphicalEffects 1.0

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: videoNavigation

    property string resourceId
    property var mediaPlayer

    width: parent.width
    height: navigator.height + navigationPanel.height
    anchors.bottom: parent.bottom

    property real cursorTickMargin: dp(10)
    property real timelineTextMargin: timeline.height - timeline.chunkBarHeight
    property real cursorWidth: dp(2)

    readonly property var _locale: Qt.locale()

    Item {
        id: navigator
        width: parent.width
        height: timeline.height + playbackController.height
        y: navigationPanel.height
        Behavior on y { SmoothedAnimation { duration: 200; reversingMode: SmoothedAnimation.Sync } }

        MouseArea {
            id: navigatorMouseArea

            anchors.fill: navigator
            drag.target: navigator
            drag.axis: Drag.YAxis
            drag.minimumY: 0
            drag.maximumY: navigationPanel.height
            drag.filterChildren: true

            property real _prevY

            onPressed: _prevY = mouse.y
            onMouseYChanged: _prevY = mouse.y

            onReleased: {
                var dir = mouse.y - _prevY

                if (dir > dp(1)) {
                    navigator.y = drag.minimumY
                } else if (dir < -dp(1)) {
                    navigator.y = drag.maximumY
                } else {
                    var mid = (drag.minimumY + drag.maximumY) / 2
                    if (navigator.y < mid)
                        navigator.y = drag.minimumY
                    else
                        navigator.y = drag.maximumY
                }
            }

            Image {
                width: timeline.width
                height: sourceSize.height
                anchors.bottom: timeline.bottom
                anchors.bottomMargin: timeline.chunkBarHeight
                sourceSize.height: timeline.height - timeline.chunkBarHeight + playbackController.height / 2
                source: "image://icon/timeline_gradient.png"
            }

            QnTimeline {
                id: timeline

                anchors.bottom: parent.bottom
                width: parent.width
                height: dp(140)

                windowEnd: (new Date(Date.now() + 5 * 60 * 1000)).getTime()
                windowStart: (new Date(Date.now() - 5 * 60 * 1000)).getTime()

                stickToEnd: mediaPlayer.atLive && !playbackController.paused

                chunkBarHeight: dp(36)
                textY: (height - chunkBarHeight) / 2

                chunkProvider: mediaPlayer.chunkProvider
                startBound: mediaPlayer.chunkProvider.bottomBound

                autoPlay: mediaPlayer.playing

                onMoveFinished: mediaPlayer.seek(position)

                onPositionTapped: mediaPlayer.seek(position)

                Connections {
                    target: mediaPlayer
                    onTimelineCorrectionRequest: {
                        if (timeline.moving)
                            return

                        timeline.correctPosition(position)
                    }
                    onTimelinePositionRequest: {
                        if (timeline.moving)
                            return

                        timeline.position = position
                    }
                    onPlayingChanged: {
                        if (timeline.moving)
                            return

                        if (!mediaPlayer.playing)
                            timeline.clearCorrection()
                    }
                }
            }

            Item {
                id: timelineMask

                anchors.fill: timeline
                visible: false

                Rectangle {
                    id: blurRectangle

                    readonly property real blurWidth: dp(36)
                    readonly property real margin: dp(18)

                    width: timeline.height - timeline.chunkBarHeight
                    height: timeline.width
                    rotation: 90
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -timeline.chunkBarHeight / 2
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                        GradientStop { position: (timeLiveLabel.x - blurRectangle.blurWidth - blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                        GradientStop { position: (timeLiveLabel.x - blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 0.0) }
                        GradientStop { position: 0.5; color: Qt.rgba(1.0, 1.0, 1.0, 0.0) }
                        GradientStop { position: (timeLiveLabel.x + timeLiveLabel.width + blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 0.0) }
                        GradientStop { position: (timeLiveLabel.x + timeLiveLabel.width + blurRectangle.blurWidth + blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                        GradientStop { position: 1.0; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                    }
                }

                Rectangle {
                    width: timeline.width
                    height: timeline.chunkBarHeight
                    anchors.bottom: parent.bottom
                    color: "#ffffffff"
                }
            }

            OpacityMask {
                anchors.fill: timeline
                source: timeline.timelineView
                maskSource: timelineMask

                Component.onCompleted: timeline.timelineView.visible = false
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
                    onClicked: {
                        mediaPlayer.playLive()
                        playbackController.paused = false
                    }
                    opacity: timeline.stickToEnd ? 0.0 : 1.0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }

                QnIconButton {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "image://icon/calendar.png"
                    onClicked: {
                        calendarPanel.date = timeline.positionDate
                        calendarPanel.show()
                    }
                }

                QnIconButton {
                    id: zoomOutButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: -width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "image://icon/minus.png"
                }

                QnIconButton {
                    id: zoomInButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "image://icon/plus.png"
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

            Text {
                id: dateLabel

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: timeline.verticalCenter
                    verticalCenterOffset: -timeline.chunkBarHeight / 2 - timeLiveLabel.height / 2
                }

                font.pixelSize: dp(20)
                font.weight: Font.Normal

                text: timeline.positionDate.toLocaleDateString(_locale, qsTr("d MMMM yyyy"))
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

                width: timeLabel.visible ? timeLabel.width : liveLabel.width
                height: timeLabel.height

                QnTimeLabel {
                    id: timeLabel
                    dateTime: timeline.positionDate
                    visible: !timeline.stickToEnd
                }

                Text {
                    id: liveLabel
                    anchors.verticalCenter: parent.verticalCenter
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
                    if (paused)
                        mediaPlayer.pause()
                    else
                        mediaPlayer.play(timeline.position)
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

    QnCalendarPanel {
        id: calendarPanel

        chunkProvider: mediaPlayer.chunkProvider
        onDatePicked: {
            hide()
            mediaPlayer.seek(date.getTime())
        }
    }
}

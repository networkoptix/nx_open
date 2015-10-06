import QtQuick 2.4
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: videoNavigation

    property string resourceId
    property var mediaPlayer

    width: parent.width
    height: navigator.height + navigationPanel.height
    anchors.bottom: parent.bottom

    QtObject {
        id: d

        function updateNavigatorPosition() {
            if (Screen.primaryOrientation == Qt.PortraitOrientation) {
                navigator.y = 0
                navigatorMouseArea.drag.target = undefined
            } else {
                navigatorMouseArea.drag.target = navigator
            }
        }
        Screen.onPrimaryOrientationChanged: updateNavigatorPosition()

        readonly property var locale: Qt.locale()
    }


    Item {
        id: navigator
        width: parent.width
        height: timeline.height + playbackController.height
        Behavior on y { SmoothedAnimation { duration: 200; reversingMode: SmoothedAnimation.Sync } }

        MouseArea {
            id: navigatorMouseArea

            anchors.fill: navigator
            drag.axis: Drag.YAxis
            drag.minimumY: 0
            drag.maximumY: navigationPanel.height
            drag.filterChildren: true

            property real prevY

            onPressed: {
                if (drag.target)
                    prevY = drag.target.y
            }
            onMouseYChanged: {
                if (drag.target)
                    prevY = drag.target.y
            }
            onReleased: {
                if (!drag.target)
                    return

                var dir = drag.target.y - prevY

                if (dir > dp(1)) {
                    drag.target.y = drag.maximumY
                } else if (dir < -dp(1)) {
                    drag.target.y = drag.minimumY
                } else {
                    var mid = (drag.minimumY + drag.maximumY) / 2
                    if (drag.target.y < mid)
                        drag.target.y = drag.minimumY
                    else
                        drag.target.y = drag.maximumY
                }
            }

            Image {
                width: timeline.width
                height: sourceSize.height
                anchors.bottom: timeline.bottom
                anchors.bottomMargin: timeline.chunkBarHeight
                sourceSize.height: timeline.height - timeline.chunkBarHeight + playbackController.height / 2
                source: "qrc:///images/timeline_gradient.png"
            }

            QnTimeline {
                id: timeline

                anchors.bottom: parent.bottom
                width: parent.width
                height: dp(150)

                windowEnd: (new Date(Date.now() + 5 * 60 * 1000)).getTime()
                windowStart: (new Date(Date.now() - 5 * 60 * 1000)).getTime()

                stickToEnd: mediaPlayer.atLive && !playbackController.paused

                chunkBarHeight: dp(32)
                textY: height - chunkBarHeight - dp(16) - dp(24)

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
                height: dp(64)
                anchors.top: timeline.bottom
                color: QnTheme.navigationPanelBackground
                clip: true

                QnButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("LIVE")
                    flat: true
                    iconic: true
                    onClicked: {
                        playbackController.paused = false
                        mediaPlayer.playLive()
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
                    onClicked: timeline.zoomOut()
                }

                QnIconButton {
                    id: zoomInButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "image://icon/plus.png"
                    onClicked: timeline.zoomIn()
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

            Item {
                id: dateTimeLabel

                height: dp(56)
                width: parent.width
                anchors.bottom: timeline.bottom
                anchors.bottomMargin: timeline.chunkBarHeight + dp(16)

                Text {
                    id: dateLabel

                    anchors.horizontalCenter: parent.horizontalCenter

                    height: dp(24)
                    font.pixelSize: sp(14)
                    font.weight: Font.Normal
                    verticalAlignment: Text.AlignVCenter

                    text: timeline.positionDate.toLocaleDateString(d.locale, qsTr("d MMMM yyyy"))
                    color: "white"

                    opacity: timeline.stickToEnd ? 0.0 : 1.0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }

                Item {
                    id: timeLiveLabel

                    anchors.horizontalCenter: parent.horizontalCenter

                    y: timeline.stickToEnd ? (parent.height - height) / 2 : parent.height - height
                    Behavior on y { NumberAnimation { duration: 200 } }

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
                        font.pixelSize: sp(32)
                        font.weight: Font.Normal
                        color: QnTheme.windowText
                        text: qsTr("LIVE")
                        visible: timeline.stickToEnd
                    }
                }
            }

            QnPlaybackController {
                id: playbackController

                anchors.verticalCenter: timeline.top
                anchors.horizontalCenter: parent.horizontalCenter

                onPausedChanged: {
                    if (paused)
                        mediaPlayer.pause()
                    else
                        mediaPlayer.play(timeline.position)
                }
            }

            Rectangle {
                color: QnTheme.playPause
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                width: dp(2)
                height: timeline.chunkBarHeight + dp(8)
            }
        }
    }

    Rectangle {
        // This rectangle guarantees the same color under android navigation buttons as under the navigation panel
        width: parent.width
        anchors.top: parent.bottom
        color: navigationPanel.color
        height: navigationPanel.height
    }

    QnCalendarPanel {
        id: calendarPanel

        chunkProvider: mediaPlayer.chunkProvider
        onDatePicked: {
            hide()
            mediaPlayer.seek(date.getTime())
        }
    }

    Component.onCompleted: d.updateNavigatorPosition()
}

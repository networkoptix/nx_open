import QtQuick 2.0

import com.networkoptix.qml 1.0

Item {
    id: root

    property alias windowStart: timeline.windowStart
    property alias windowEnd: timeline.windowEnd
    property alias position: timeline.position
    property alias positionDate: timeline.positionDate
    property alias chunkBarHeight: timeline.chunkBarHeight
    property alias textY: timeline.textY
    property alias stickToEnd: timeline.stickToEnd
    property alias chunkProvider: timeline.chunkProvider
    property alias startBound: timeline.startBound
    property alias autoPlay: timeline.autoPlay
    readonly property bool dragging: timeline.dragging
    readonly property bool moving: timeline.moving
    readonly property var timelineView: timeline

    signal dragFinished()
    signal moveFinished()
    signal positionTapped(real position)

    function zoomIn() {
        timeline.zoomIn()
    }

    function zoomOut() {
        timeline.zoomOut()
    }

    function correctPosition(position) {
        timelineView.correctPosition(position)
    }

    function clearCorrection() {
        timelineView.clearCorrection()
    }

    QnTimelineView {
        id: timeline
        anchors.fill: parent

        property bool moving: false
        property bool dragging: false

        textColor: QnTheme.timelineText
        chunkColor: QnTheme.timelineChunk

        timeZoneShift: -(new Date()).getTimezoneOffset() * 60 * 1000

        font.pixelSize: sp(16)

        onMoveFinished: {
            moving = false
            root.moveFinished()
        }
    }

    PinchArea {
        id: pinchArea

        anchors.fill: parent
        pinch.minimumRotation: 0
        pinch.maximumRotation: 0
        pinch.dragAxis: Qt.XAxis

        onPinchStarted: {
            timeline.moving = true
            timeline.dragging = true
            timeline.startPinch(pinch.center.x, pinch.scale)
        }
        onPinchUpdated: {
            timeline.updatePinch(pinch.center.x, pinch.scale)
        }
        onPinchFinished: {
            timeline.dragging = false
            timeline.finishPinch(pinch.center.x, pinch.scale)
            root.dragFinished()
        }

        MouseArea {
            id: mouseArea

            property int pressX: -1

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            drag.threshold: dp(10)

            onPressed: {
                pressX = mouseX
            }
            onMouseXChanged: {
                if (pressX != -1 && Math.abs(pressX - mouseX) > drag.threshold) {
                    preventStealing = true
                    timeline.moving = true
                    timeline.dragging = true
                    timeline.startDrag(pressX)
                    pressX = -1
                }
                timeline.updateDrag(mouse.x)
            }
            onReleased: {
                preventStealing = false
                pressX = -1
                timeline.finishDrag(mouse.x)
                timeline.dragging = false

                if (timeline.moving)
                    root.dragFinished()
            }
            onClicked: {
                if (moving)
                    return

                positionTapped(timeline.positionAtX(mouse.x))
            }
            onWheel: {
                if (wheel.angleDelta.y > 0)
                    timeline.zoomIn()
                else if (wheel.angleDelta.y < 0)
                    timeline.zoomOut()
            }
        }
    }
}


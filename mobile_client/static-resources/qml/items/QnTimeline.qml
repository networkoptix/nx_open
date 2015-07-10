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
    readonly property bool dragging: mouseArea.pressed || pinchArea.pinch.active
    readonly property bool moving: timeline.moving
    readonly property var timelineView: timeline

    signal dragFinished()
    signal moveFinished()

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

        textColor: QnTheme.timelineText
        chunkColor: QnTheme.timelineChunk

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
            timeline.startPinch(pinch.center.x, pinch.scale)
        }
        onPinchUpdated: {
            timeline.updatePinch(pinch.center.x, pinch.scale)
        }
        onPinchFinished: {
            timeline.finishPinch(pinch.center.x, pinch.scale)
            root.dragFinished()
        }

        MouseArea {
            id: mouseArea

            property int pressX: -1

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton

            onPressed: {
                pressX = mouseX
            }
            onMouseXChanged: {
                if (pressX != -1 && Math.abs(pressX - mouseX) > drag.threshold) {
                    preventStealing = true
                    timeline.moving = true
                    timeline.startDrag(pressX)
                    pressX = -1
                }
                timeline.updateDrag(mouse.x)
            }
            onReleased: {
                preventStealing = false
                pressX = -1
                timeline.finishDrag(mouse.x)

                if (timeline.moving)
                    root.dragFinished()
            }
            onClicked: {
                timeline.position = timeline.positionAtX(mouseArea.pressX)
                root.moveFinished()
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


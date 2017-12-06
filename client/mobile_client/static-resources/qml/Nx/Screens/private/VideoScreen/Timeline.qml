import QtQuick 2.0
import Nx 1.0
import com.networkoptix.qml 1.0

Item
{
    id: root

    property alias defaultWindowSize: timeline.defaultWindowSize
    property alias windowStart: timeline.windowStart
    property alias windowEnd: timeline.windowEnd
    property alias windowSize: timeline.windowSize
    property alias position: timeline.position
    property alias positionDate: timeline.positionDate
    property alias chunkBarHeight: timeline.chunkBarHeight
    property alias textY: timeline.textY
    property alias stickToEnd: timeline.stickToEnd
    property alias chunkProvider: timeline.chunkProvider
    property alias startBound: timeline.startBound
    property alias autoPlay: timeline.autoPlay
    property alias autoReturnToBounds: timeline.autoReturnToBounds
    readonly property bool dragging: timeline.dragging
    readonly property bool moving: timeline.moving
    readonly property var timelineView: timeline

    signal positionTapped(real position)

    function zoomIn()
    {
        timeline.zoomIn()
    }

    function zoomOut()
    {
        timeline.zoomOut()
    }

    function correctPosition(position)
    {
        timelineView.correctPosition(position)
    }

    function clearCorrection()
    {
        timelineView.clearCorrection()
    }

    function jumpTo(position)
    {
        timeline.setPositionImmediately(position)
    }

    QnTimelineView
    {
        id: timeline
        anchors.fill: parent

        property bool moving: false
        property bool dragging: false

        textColor: ColorTheme.transparent(ColorTheme.contrast16, 0.7)
        chunkColor: ColorTheme.green_main

        timeZoneShift: -(new Date()).getTimezoneOffset() * 60 * 1000

        font.pixelSize: 15

        onMoveFinished: moving = false
    }

    PinchArea
    {
        id: pinchArea

        anchors.fill: parent
        pinch.minimumRotation: 0
        pinch.maximumRotation: 0
        pinch.dragAxis: Qt.XAxis

        onPinchStarted:
        {
            timeline.startZoom(pinch.scale)
            timeline.dragging = false
        }
        onPinchUpdated:
        {
            timeline.updateZoom(pinch.scale)
        }
        onPinchFinished:
        {
            timeline.finishZoom(pinch.scale)
        }

        MouseArea
        {
            id: mouseArea

            property int pressX: -1

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            drag.threshold: 10

            onPressed:
            {
                pressX = mouseX
            }
            onMouseXChanged:
            {
                if (pressX != -1 && Math.abs(pressX - mouseX) > drag.threshold)
                {
                    preventStealing = true
                    timeline.moving = true
                    timeline.dragging = true
                    timeline.startDrag(pressX)
                    pressX = -1
                }
                timeline.updateDrag(mouse.x)
            }
            onReleased:
            {
                preventStealing = false
                pressX = -1

                if (timeline.moving)
                {
                    timeline.dragging = false
                    timeline.finishDrag(mouse.x)
                }
                else
                {
                    positionTapped(timeline.positionAtX(mouse.x))
                }
            }
            onWheel:
            {
                if (wheel.angleDelta.y > 0)
                    timeline.zoomIn()
                else if (wheel.angleDelta.y < 0)
                    timeline.zoomOut()
            }
        }
    }
}


// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Nx.Core
import Nx.Mobile

Item
{
    id: control

    property alias displayOffset: timeline.displayOffset
    property alias defaultWindowSize: timeline.defaultWindowSize
    property alias windowSize: timeline.windowSize
    property alias position: timeline.position
    property alias chunkBarHeight: timeline.chunkBarHeight
    property alias textY: timeline.textY
    property alias stickToEnd: timeline.stickToEnd
    property alias chunkProvider: timeline.chunkProvider
    property alias startBound: timeline.startBound
    property alias autoReturnToBounds: timeline.autoReturnToBounds
    property alias motionSearchMode: timeline.motionSearchMode
    property alias changingMotionRoi: timeline.changingMotionRoi
    property alias rightChunksOffsetMs: timeline.rightChunksOffsetMs

    readonly property bool dragging: timeline.dragging
    readonly property bool moving: timeline.moving
    readonly property var timelineView: timeline
    property real bottomOverlap: 0

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

    QnTimelineView
    {
        id: timeline
        anchors.fill: parent

        property bool moving: false
        property bool dragging: false

        textColor: ColorTheme.transparent(ColorTheme.colors.light4, 0.4)

        chunkBarColor: ColorTheme.transparent(ColorTheme.colors.green_d2, 0.4)
        chunkColor: ColorTheme.colors.green_core
        loadingChunkColor: ColorTheme.transparent(chunkColor, 0.6)
        motionModeChunkColor: ColorTheme.transparent(ColorTheme.colors.green_d2, 0.6)
        motionColor: ColorTheme.colors.red_l2
        motionLoadingColor: ColorTheme.transparent(motionColor, 0.6)

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

            width: parent.width
            height: parent.height + control.bottomOverlap
            acceptedButtons: Qt.LeftButton
            drag.threshold: 10

            onPressed:
            {
                pressX = mouseX
            }

            onMouseXChanged: (mouse) =>
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

            onReleased: (mouse) =>
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

            onWheel: (wheel) =>
            {
                if (wheel.angleDelta.y > 0)
                    timeline.zoomIn()
                else if (wheel.angleDelta.y < 0)
                    timeline.zoomOut()
            }
        }
    }
}

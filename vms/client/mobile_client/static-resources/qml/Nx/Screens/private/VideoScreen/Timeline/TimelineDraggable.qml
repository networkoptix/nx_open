// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Items

// Interactive area of a timeline item that can be tapped and dragged vertically.
// Must stay visible while dragged, otherwise the drag is cancelled; its geometry may change.
//
// An item can also be "pulled in": the pointer must first travel `pullInThreshold` pixels towards
// the inside of the time window (down from the upper half of `timeScale`, up from the lower half)
// before the drag starts. Until then, nothing is positioned and `dragged` is not emitted; when the
// threshold is crossed, positioning starts and `dragged` is emitted at once. If `pullInEnabled`
// becomes false meanwhile (e.g. the item comes into the window by itself), the pull-in turns into
// a regular drag from the current pointer position.
Item
{
    id: draggable

    required property VerticalTimeScale timeScale

    property bool interactive: true
    property bool dragEnabled: true

    // Whether a drag must be pulled in first.
    property bool pullInEnabled: false
    property real pullInThreshold: 64

    // From the press (or from crossing the pull-in threshold) until release or cancellation.
    readonly property alias positioning: d.positioning

    // From the drag start or a long press until the pull-in threshold is crossed, or release or
    // cancellation. (Internally the pull-in starts on press, but a tap is not reported as one.)
    readonly property bool pullingIn: d.pullingIn && (dragHandler.active
        || tapHandler.timeHeld >= tapHandler.longPressThreshold)

    // Pointer travel towards the inside of the time window relative to `pullInThreshold`, from 0
    // to 1, while pulling in; 0 otherwise.
    readonly property real pullInFraction: pullingIn ? d.pullInFraction : 0

    // The pointer has moved past the drag threshold.
    readonly property alias dragging: dragHandler.active

    // From the press until release or cancellation of a tap, a pull-in or a drag.
    readonly property bool pressed: tapHandler.pressed || d.pullingIn || positioning

    readonly property point scenePosition: dragHandler.centroid.scenePosition

    // Vertical position of the press (or of the pointer when a pull-in turned into a regular
    // drag), in `timeScale` coordinates.
    readonly property alias pressY: d.pressY

    // Emitted when the pointer moves while dragged, and when the time window moves under it.
    // `y` is in `timeScale` coordinates.
    signal dragged(real y)

    signal tapped()

    // A disabled handler keeps its passive grab and isn't notified, so the gesture is ended here.
    onInteractiveChanged:
    {
        if (!interactive)
            d.reset()
    }

    onPullInEnabledChanged:
    {
        if (pullInEnabled || !d.pullingIn)
            return

        // Positioning starts before pulling in ends, so `pressed` stays true.
        if (dragEnabled)
        {
            d.pressY = d.pointerY()
            d.positioning = true
        }

        d.pullingIn = false
        d.pullInFraction = 0
    }

    TapHandler
    {
        id: tapHandler

        acceptedButtons: Qt.LeftButton
        enabled: draggable.interactive

        onTapped:
            draggable.tapped()
    }

    DragHandler
    {
        id: dragHandler

        acceptedButtons: Qt.LeftButton
        maximumPointCount: 1
        dragThreshold: 4
        target: null

        enabled: draggable.interactive
            && (d.positioning || d.pullingIn || draggable.dragEnabled)

        onActiveChanged:
        {
            if (!active)
                d.reset()
        }

        onCentroidChanged:
        {
            // Becoming active again after a cancellation doesn't restart positioning: no effect.
            if (d.pullingIn)
                d.updatePullIn()
            else if (active && d.positioning)
                d.emitDragged()
        }

        onGrabChanged: (transition, eventPoint) =>
        {
            switch (transition)
            {
                case PointerDevice.GrabPassive:
                    d.pressY = draggable.timeScale.mapFromItem(
                        null, eventPoint.scenePressPosition).y

                    if (draggable.pullInEnabled)
                    {
                        d.pullInDirection = d.pressY < draggable.timeScale.height / 2 ? 1 : -1
                        d.pullingIn = true
                    }
                    else
                    {
                        d.positioning = true
                    }
                    break

                case PointerDevice.UngrabPassive:
                case PointerDevice.CancelGrabPassive:
                    d.reset()
                    break
            }
        }
    }

    // While positioning, the time window may move under the pointer (e.g. following live): the
    // drag is re-applied, from the press position until the drag threshold is passed.
    Connections
    {
        target: draggable.timeScale
        enabled: d.positioning

        function onStartTimeMsChanged()
        {
            d.emitDragged()
        }

        function onDurationMsChanged()
        {
            d.emitDragged()
        }
    }

    QtObject
    {
        id: d

        property bool positioning: false
        property bool pullingIn: false
        property real pullInFraction: 0
        property int pullInDirection: 1 //< 1 is down, -1 is up.
        property real pressY: 0

        function pointerY()
        {
            return draggable.timeScale.mapFromItem(null, dragHandler.centroid.scenePosition).y
        }

        function emitDragged()
        {
            draggable.dragged(dragHandler.active ? pointerY() : pressY)
        }

        function updatePullIn()
        {
            const y = pointerY()
            const travel = (y - pressY) * pullInDirection

            if (travel < draggable.pullInThreshold)
            {
                pullInFraction = MathUtils.bound(0, travel / draggable.pullInThreshold, 1)
                return
            }

            // Positioning starts before pulling in ends, so `pressed` stays true.
            positioning = true
            pullingIn = false
            pullInFraction = 0
            draggable.dragged(y)
        }

        function reset()
        {
            positioning = false
            pullingIn = false
            pullInFraction = 0
        }
    }
}

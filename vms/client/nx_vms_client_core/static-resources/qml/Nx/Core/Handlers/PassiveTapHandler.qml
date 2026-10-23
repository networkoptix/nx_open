// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

// Reports taps without interfering with the items under the parent item: presses still reach
// them, so they can be dragged, pinched, etc. TapHandler with the DragThreshold gesture policy is
// passive only for touch: it keeps mouse presses accepted, blocking the items under it from the
// mouse (QTBUG-126812). PointHandler never accepts the event points.
PointHandler
{
    id: handler

    // Distance a press must move to become a drag rather than a tap.
    property var startDragDistance: Application.styleHints.startDragDistance

    signal tapped(eventPoint eventPoint)

    onGrabChanged: (transition, eventPoint) =>
    {
        // A tap ends with a normal release, not with a cancellation or a takeover by a drag.
        if (transition !== PointerDevice.UngrabPassive || eventPoint.state !== EventPoint.Released)
            return

        // Only the release point is compared with the press point: a press which moved away and
        // came back is still a tap.
        const offset = Qt.vector2d(eventPoint.position.x - eventPoint.pressPosition.x,
            eventPoint.position.y - eventPoint.pressPosition.y)
        if (offset.length() > startDragDistance)
            return

        const heldMs = eventPoint.timestamp - eventPoint.pressTimestamp
        if (heldMs >= Application.styleHints.mousePressAndHoldInterval)
            return

        handler.tapped(eventPoint)
    }
}

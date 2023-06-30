// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Nx.Core
import nx.vms.client.desktop

Instrument
{
    id: rotationInstrument

    property Item target: null
    property real roundingAngle: 90
    property real roundingThresholdAngle: 5

    signal started()
    signal finished()

    readonly property bool rotating: _started

    property real _startAngle
    property real _startRotation
    property bool _started: false

    onMousePress:
    {
        if (!target)
            return

        if (mouse.button !== Qt.LeftButton || mouse.modifiers !== Qt.AltModifier)
            return

        start(mouse.position)

        mouse.accepted = true
    }

    onMouseRelease:
    {
        if (!target || !_started)
            return

        move(mouse.position)
        stop()

        mouse.accepted = true
    }

    onMouseMove:
    {
        if (!target || !_started)
            return

        move(mouse.position)

        mouse.accepted = true
    }

    onRotatingChanged:
    {
        if (rotating)
            started()
        else
            finished()
    }

    function start(position, caller)
    {
        if (!target)
            return

        if (caller)
            position = caller.mapToItem(item, position.x, position.y)

        _startAngle = calculateAngle(position)
        _startRotation = target.rotation
        _started = true
    }

    function stop()
    {
        if (!target || !_started)
            return

        _started = false
    }

    function move(position, caller)
    {
        if (!target || !_started)
            return

        if (caller)
            position = caller.mapToItem(item, position.x, position.y)

        let rotation = normalizedAngle(_startRotation + calculateAngle(position) - _startAngle)
        const roundedRotation = Math.round(rotation / roundingAngle) * roundingAngle
        if (Math.abs(rotation - roundedRotation) < roundingThresholdAngle)
            rotation = roundedRotation

        target.rotation = rotation
    }

    function calculateAngle(position)
    {
        if (!target)
            return

        const angle = -Math.atan2(position.x - item.width / 2, position.y - item.height / 2)
        return MathUtils.toDegrees(angle)
    }

    function normalizedAngle(angle)
    {
        return (angle + 360) % 360
    }
}

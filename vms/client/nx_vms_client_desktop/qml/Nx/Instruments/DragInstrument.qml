// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx 1.0
import nx.client.core 1.0
import nx.client.desktop 1.0

Instrument
{
    id: dragInstrument

    property Item target: null
    property real minX: -Infinity
    property real maxX: Infinity
    property real minY: -Infinity
    property real maxY: Infinity
    property real threshold: 0

    readonly property bool dragging: _started

    signal started()
    signal finished()

    property point _pressPosition
    property bool _started: false
    property point _pressItemPosition

    onMousePress:
    {
        if (!target)
            return

        _pressPosition = mouse.globalPosition
        _pressItemPosition = Qt.point(target.x, target.y)
        mouse.accepted = true
    }

    onMouseRelease:
    {
        if (!_started || !target)
            return

        _handleMove(mouse.globalPosition.x, mouse.globalPosition.y)
        _started = false
        dragInstrument.finished()

        mouse.accepted = true
    }

    onMouseMove:
    {
        if (!target)
            return

        if (!_started)
        {
            if (Geometry.length(Geometry.cwiseSub(
                _pressPosition, mouse.globalPosition)) < threshold)
            {
                return
            }

            _started = true
            dragInstrument.started()
        }

        _handleMove(mouse.globalPosition.x, mouse.globalPosition.y)
        mouse.accepted = true
    }

    function _handleMove(x, y)
    {
        const pressLocalPos = target.mapFromGlobal(_pressPosition.x, _pressPosition.y)
        const localPos = target.mapFromGlobal(x, y)

        var newX = MathUtils.bound(
            minX, _pressItemPosition.x + localPos.x - pressLocalPos.x, maxX)
        var newY = MathUtils.bound(
            minY, _pressItemPosition.y + localPos.y - pressLocalPos.y, maxY)

        target.x = newX
        target.y = newY
    }
}

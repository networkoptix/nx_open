import QtQuick 2.6
import Nx 1.0
import Nx.Client.Desktop.Ui.Scene 1.0

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

    function start(position)
    {
        if (!target)
            return

        _startAngle = calculateAngle(position)
        _startRotation = target.rotation
        _started = true
    }

    function stop()
    {
        if (!target)
            return

        _started = false
    }

    function move(position)
    {
        if (!target)
            return

        var rotation = normalizedAngle(_startRotation + calculateAngle(position) - _startAngle)
        var roundedRotation = Math.round(rotation / roundingAngle) * roundingAngle
        if (Math.abs(rotation - roundedRotation) < roundingThresholdAngle)
            rotation = roundedRotation

        target.rotation = rotation
    }

    function calculateAngle(position)
    {
        if (!target)
            return

        var angle = -Math.atan2(position.x - item.width / 2, position.y - item.height / 2)
        return MathUtils.toDegrees(angle)
    }

    function normalizedAngle(angle)
    {
        return (angle + 180) % 360 - 180
    }
}

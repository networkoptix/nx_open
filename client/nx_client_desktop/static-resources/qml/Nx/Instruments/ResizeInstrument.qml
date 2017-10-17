import QtQuick 2.6
import Nx 1.0
import nx.client.core 1.0
import nx.client.desktop 1.0

Instrument
{
    id: resizeInstrument

    property Item target: null
    property CursorManager cursorManager: null
    property real minWidth: 0
    property real maxWidth: Infinity
    property real minHeight: 0
    property real maxHeight: Infinity
    property real frameWidth: 4

    readonly property bool resizing: _started

    signal started()
    signal finished()

    property point _pressPosition
    property rect _startGeometry
    property bool _started: false

    onMousePress:
    {
        if (!target)
            return

        _pressPosition = mouse.globalPosition
        _startGeometry = Qt.rect(target.x, target.y, target.width, target.height)

        var point = target.mapFromItem(item, mouse.position.x, mouse.position.y)
        var frameSection = FrameSection.frameSection(
            point, Qt.rect(0, 0, target.width, target.height), frameWidth)
    }

    onMouseRelease:
    {
        if (!_started || !target)
            return
    }

    onMouseMove:
    {
        if (!_started || !target)
            return
    }

    onHoverLeave:
    {
        if (!cursorManager)
            return

        cursorManager.unsetCursor(this)
    }

    onHoverMove:
    {
        if (!target || !cursorManager)
            return

        var point = target.mapFromItem(item, hover.position.x, hover.position.y)
        var frameSection = FrameSection.frameSection(
            point, Qt.rect(0, 0, target.width, target.height), frameWidth)
        cursorManager.setCursorForFrameSection(this, frameSection, target.rotation)
    }
}

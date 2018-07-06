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
    property size _startSize
    property point _startPosition
    property point _origin
    property int _frameSection
    property bool _started: false

    onMousePress:
    {
        if (!target)
            return

        var point = target.mapFromItem(item, mouse.position.x, mouse.position.y)
        _frameSection = FrameSection.frameSection(
            point, Qt.rect(0, 0, target.width, target.height), frameWidth)

        if (_frameSection === FrameSection.NoSection)
            return

        _pressPosition = mouse.globalPosition
        _startSize = Qt.size(target.width, target.height)
        _startPosition = Qt.point(target.x, target.y)
        _origin = FrameSection.sectionCenterPoint(
            Qt.rect(0, 0, target.width, target.height),
            FrameSection.oppositeSection(_frameSection))
        _origin = Geometry.cwiseSub(
            target.mapToItem(target.parent, _origin.x, _origin.y), _startPosition)

        _started = true

        mouse.accepted = true
    }

    onMouseRelease:
    {
        if (!_started || !target)
            return

        _started = false

        mouse.accepted = true
    }

    onMouseMove:
    {
        if (!_started || !target)
            return

        var delta = Geometry.cwiseSub(mouse.globalPosition, _pressPosition)
        delta = Geometry.rotated(delta, Qt.point(0, 0), -target.rotation)
        delta = FrameSection.sizeDelta(delta, _frameSection)

        var aspectRatio = Geometry.aspectRatio(_startSize)

        var newSize = Geometry.cwiseAdd(_startSize, delta)
        newSize.width = MathUtils.bound(minWidth, newSize.width, maxWidth)
        newSize.height = MathUtils.bound(minHeight, newSize.height, maxHeight)

        if (delta.width === 0)
            newSize.width = newSize.height * aspectRatio
        else if (delta.height === 0)
            newSize.height = newSize.width / aspectRatio
        else
            newSize = Geometry.expanded(aspectRatio, newSize, Qt.KeepAspectRatioByExpanding)

        var origin = FrameSection.sectionCenterPoint(
            Qt.rect(0, 0, newSize.width, newSize.height),
            FrameSection.oppositeSection(_frameSection))
        origin = Geometry.rotated(
            origin, Qt.point(newSize.width / 2, newSize.height / 2), target.rotation)
        target.x = _startPosition.x + _origin.x - origin.x
        target.y = _startPosition.y + _origin.y - origin.y

        target.width = newSize.width
        target.height = newSize.height

        mouse.accepted = true
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

    onResizingChanged:
    {
        if (resizing)
            started()
        else
            finished()
    }
}

import QtQuick 2.6
import Nx 1.0
import Nx.Client.Desktop.Ui.Scene 1.0
import Nx.Client.Core.Ui 1.0

Instrument
{
    id: resizeInstrument

    property Item target: null
    property real minWidth: 0
    property real maxWidth: Infinity
    property real minHeight: 0
    property real maxHeight: Infinity
    property real frameWidth: 4

    readonly property bool resizing: _started

    signal started()
    signal finished()

    property point _pressPosition
    property bool _started: false

    onMousePress:
    {
        if (!target)
            return

        _pressPosition = mouse.globalPosition
        var frameSection = FrameSection.frameSection(
            mouse.position, Qt.rect(0, 0, item.width, item.height), frameWidth)
    }

    onMouseRelease:
    {
        if (!_started || !target)
            return
    }

    onMouseMove:
    {
        if (!target)
            return

        if (!_started)
        {
            _started = true
            resizeInstrument.started()
        }
    }
}

import QtQuick 2.6

import Nx 1.0

MouseArea
{
    id: resizer

    property Item target: null //< Must be a sibling of the resizer.

    property int edge: Qt.LeftEdge

    readonly property bool dragging: drag.active

    readonly property int orientation: (edge === Qt.LeftEdge || edge === Qt.RightEdge)
        ? Qt.Vertical
        : Qt.Horizontal

    property real handleWidth: 6

    signal dragPositionChanged(real pos)

    cursorShape: orientation === Qt.Vertical ? Qt.SplitHCursor : Qt.SplitVCursor

    drag.smoothed: false
    drag.threshold: 0

    // Private.

    drag.target: this
    drag.axis: orientation === Qt.Vertical ? Drag.XAxis : Drag.YAxis

    anchors.left: !target || edge === Qt.LeftEdge
        ? undefined
        : (edge === Qt.RightEdge
            ? (drag.active ? undefined : target.right)
            : target.left)

    anchors.right: !target || edge === Qt.RightEdge
        ? undefined
        : (edge === Qt.LeftEdge
            ? (drag.active ? undefined : target.left)
            : target.right)

    anchors.top: !target || edge === Qt.TopEdge
        ? undefined
        : (edge === Qt.BottomEdge
            ? (drag.active ? undefined : target.bottom)
            : target.top)

    anchors.bottom: !target || edge === Qt.BottomEdge
        ? undefined
        : (edge === Qt.TopEdge
            ? (drag.active ? undefined : target.top)
            : target.bottom)

    width: handleWidth
    height: handleWidth

    onXChanged:
    {
        if (!drag.active)
            return

        switch (edge)
        {
            case Qt.LeftEdge:
                dragPositionChanged(x)
                break

            case Qt.RightEdge:
                dragPositionChanged(x + width)
                break
        }
    }

    onYChanged:
    {
        if (!drag.active)
            return

        switch (edge)
        {
            case Qt.TopEdge:
                dragPositionChanged(y)
                break

            case Qt.BottomEdge:
                dragPositionChanged(y + height)
                break
        }
    }
}

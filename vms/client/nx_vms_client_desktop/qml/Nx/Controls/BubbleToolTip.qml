// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Controls 1.0
import Nx 1.0
import Nx.Core 1.0

import nx.vms.client.desktop 1.0

NxObject
{
    id: toolTip

    property alias contentItem: bubble.contentItem

    property int orientation: Qt.Horizontal

    /** An item the tooltip should point at. */
    property Item target: null

    /** Rectangle to enclose the bubble within, in coordinates relative to `target`. */
    property rect enclosingRect: Qt.rect(0, 0, 0, 0) //< No limitation by default.

    /**
     * Minimum required intersection between the target and the enclosing rect the direction
     * orthogonal to orientation (for tooltip to be shown). 0 - default, half target rect is used.
     */
    property real minIntersection: 0

    property alias color: bubble.color
    property alias textColor: bubble.textColor

    property alias text: bubble.text
    property alias font: bubble.font

    enum State
    {
        Shown,
        Hidden,
        Suppressed //< Temporarily hidden, restored at next mouse move event.
    }

    // Logical state.
    readonly property alias state: d.state

    // Physical visibility: fading in, fading out or completely visible.
    readonly property alias visible: bubble.visible

    function show() { d.setState(BubbleToolTip.Shown) }

    function hide(immediately) { d.setState(BubbleToolTip.Hidden, immediately) }

    function suppress(immediately) { d.setState(BubbleToolTip.Suppressed, immediately) }

    // Implementation.

    EmbeddedPopup
    {
        id: popup

        viewport: windowContext.mainWindow

        visible: bubble.visible

        width: bubble.boundingRect.width
        height: bubble.boundingRect.height

        contentItem: Item
        {
            implicitWidth: popup.width
            implicitHeight: popup.height

            Bubble
            {
                id: bubble

                x: -boundingRect.x
                y: -boundingRect.y

                visible: effectivelyShown && !!parameters

                readonly property var parameters:
                {
                    if (!toolTip.target)
                        return undefined

                    const targetRect = Qt.rect(0, 0, toolTip.target.width, toolTip.target.height)

                    return calculateParameters(toolTip.orientation, targetRect,
                        toolTip.enclosingRect, toolTip.minIntersection)
                }

                onParametersChanged:
                {
                    if (!parameters)
                        return

                    pointerEdge = parameters.pointerEdge
                    normalizedPointerPos = parameters.normalizedPointerPos
                    popup.parent = toolTip.target
                    popup.x = bubble.parameters.x + bubble.boundingRect.x
                    popup.y = bubble.parameters.y + bubble.boundingRect.y
                    popup.raise()
                }
            }
        }
    }

    NxObject
    {
        id: d
        property int state: BubbleToolTip.Hidden

        function setState(value, immediately)
        {
            // No early exit if state == value because we want to update animation or z-order.

            // Can't suppress a hidden tooltip.
            if (value === BubbleToolTip.Suppressed && state === BubbleToolTip.Hidden)
                value = BubbleToolTip.Hidden

            state = value

            if (state === BubbleToolTip.Shown)
            {
                bubble.show()
                popup.raise()
            }
            else
            {
                bubble.hide(immediately)
            }
        }

        MouseSpy.onMouseMove:
        {
            if (d.state === BubbleToolTip.Suppressed)
                d.setState(BubbleToolTip.Shown)
        }

        MouseSpy.onMousePress:
        {
            if (d.state === BubbleToolTip.Shown)
                d.setState(BubbleToolTip.Suppressed)
        }
    }
}

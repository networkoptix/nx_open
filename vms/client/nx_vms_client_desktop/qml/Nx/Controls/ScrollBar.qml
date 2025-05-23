// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.0

import Nx.Core 1.0

ScrollBar
{
    id: control

    property color thumbColor: ColorTheme.colors.dark13
    padding: 0

    visible: policy === ScrollBar.AlwaysOn || (policy === ScrollBar.AsNeeded && size < 1.0)

    // Use `scrollBar.active` to check that a position change is caused by user interaction
    // or by a scrollBy/scrollBySteps call, rather than by a change in size or parameters.

    function scrollBy(delta)
    {
        const wasActive = active
        active = true
        position = MathUtils.bound(0.0, position + delta, 1.0 - size)
        active = wasActive
    }

    function scrollBySteps(steps)
    {
        scrollBy(steps * (stepSize || 0.1))
    }

    background: Rectangle
    {
        color: ColorTheme.colors.dark9
    }

    contentItem: Rectangle
    {
        id: thumb

        implicitWidth: 8
        implicitHeight: 8

        color:
        {
            const color = control.thumbColor
            if (control.pressed)
                return ColorTheme.lighter(color, 1)
            if (control.hovered)
                return ColorTheme.lighter(color, 2)
            return color
        }
    }
}

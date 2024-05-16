// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import Nx.Core 1.0

Rectangle
{
    property var control: null

    readonly property bool warningState:
        control.hasOwnProperty("warningState") && control.warningState

    color:
    {
        const color = ColorTheme.shadow

        if (control && control.readOnly)
            return ColorTheme.transparent(ColorTheme.shadow, 0.2)

        if (control && control.activeFocus)
            return ColorTheme.darker(color, 1)

        return ColorTheme.shadow
    }

    border.color:
    {
        const color = ColorTheme.shadow

        if (control)
        {
            if (warningState)
            {
                return ColorTheme.transparent(
                    ColorTheme.colors.red_l2, control.readOnly ? 0.4 : 1.0)
            }

            if (control.readOnly)
                return ColorTheme.transparent(color, 0.4)

            if (control.activeFocus)
                return ColorTheme.darker(color, 3)
        }

        return ColorTheme.darker(color, 1)
    }

    radius: 1

    Rectangle
    {
        width: parent.width
        height: 2
        color: parent.border.color
        visible: control && control.activeFocus && !warningState
    }

    opacity: enabled ? 1.0 : 0.3
}

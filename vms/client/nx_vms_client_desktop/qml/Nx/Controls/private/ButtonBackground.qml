// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import Nx.Core 1.0

Rectangle
{
    id: control

    property bool hovered: false
    property bool pressed: false
    property bool flat: false

    property color backgroundColor: ColorTheme.button
    property color hoveredColor: ColorTheme.lighter(backgroundColor, 1)
    property color pressedColor: ColorTheme.darker(backgroundColor, 1)
    property color outlineColor: ColorTheme.darker(backgroundColor, 2)

    color: (enabled && hovered && !pressed)
        ? hoveredColor
        : pressed
            ? pressedColor
            : flat
                ? "transparent"
                : backgroundColor

    opacity: enabled ? 1.0 : 0.3

    radius: 2

    Rectangle
    {
        width: parent.width
        height: 1
        color: ColorTheme.darker(outlineColor, 1)
        visible: pressed
    }

    Rectangle
    {
        width: parent.width - 2
        height: 1
        x: 1
        y: parent.height - 1
        color: ColorTheme.lighter(outlineColor, hovered ? 1 : 0)
        visible: !pressed && (!flat || hovered)
    }
}

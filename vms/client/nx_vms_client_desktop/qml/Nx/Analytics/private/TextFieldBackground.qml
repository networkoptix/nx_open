// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0

Rectangle
{
    id: background

    readonly property bool focused: parent && parent.activeFocus
    readonly property bool hovered: parent && parent.hovered

    color: focused
        ? ColorTheme.colors.dark3
        : (hovered ? ColorTheme.colors.dark6 : ColorTheme.colors.dark5)

    Rectangle
    {
        id: shadow

        width: background.width - border.width
        height: background.height - border.width
        visible: background.focused

        color: "transparent"
        border.color: ColorTheme.colors.dark2
    }

    Rectangle
    {
        id: border

        width: background.width
        height: background.height
        color: "transparent"
        border.color: ColorTheme.colors.dark6
    }
}

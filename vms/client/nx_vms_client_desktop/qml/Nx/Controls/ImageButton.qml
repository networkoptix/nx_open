// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

AbstractButton
{
    id: button

    property alias radius: rectangle.radius

    property color normalBackground: "transparent"
    property color normalForeground: ColorTheme.windowText
    property color hoveredBackground: ColorTheme.transparent(ColorTheme.windowText, 0.1)
    property color hoveredForeground: normalForeground
    property color pressedBackground: ColorTheme.transparent(ColorTheme.colors.dark1, 0.1)
    property color pressedForeground: normalForeground

    // The most common defaults.
    icon.width: 20
    icon.height: 20

    icon.color: down || checked
        ? pressedForeground
        : (hovered ? hoveredForeground : normalForeground)

    background: Rectangle
    {
        id: rectangle
        color: down || checked
            ? pressedBackground
            : (hovered ? hoveredBackground : normalBackground)
    }

    contentItem: IconImage
    {
        id: image

        name: button.icon.name
        color: button.icon.color
        source: button.icon.source
        sourceSize: Qt.size(button.icon.width, button.icon.height)
    }
}

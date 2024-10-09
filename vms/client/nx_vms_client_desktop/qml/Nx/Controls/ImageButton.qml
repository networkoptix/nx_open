// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.desktop

AbstractButton
{
    id: button

    property color primaryColor: icon.color
    property color secondaryColor
    property color tertiaryColor

    property color backgroundColor: down || checked
        ? ColorTheme.transparent(ColorTheme.colors.dark1, 0.1)
        : (hovered ? ColorTheme.transparent(ColorTheme.windowText, 0.1) : "transparent")
    property color borderColor: "transparent"

    property real radius: 2

    // The most common defaults.
    icon.width: 20
    icon.height: 20

    icon.color: ColorTheme.windowText

    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    background: Rectangle
    {
        radius: button.radius
        color: button.backgroundColor
        border.color: borderColor
    }

    contentItem: ColoredImage
    {
        name: button.icon.name
        primaryColor: button.primaryColor
        secondaryColor: button.secondaryColor
        tertiaryColor: button.tertiaryColor
        sourcePath: button.icon.source
        sourceSize: Qt.size(button.icon.width, button.icon.height)
    }

    FocusFrame
    {
        anchors.fill: button
        visible: button.visualFocus
    }
}

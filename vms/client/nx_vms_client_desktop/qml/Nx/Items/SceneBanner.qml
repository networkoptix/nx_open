// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx.Core 1.0

Control
{
    id: control

    property alias text: textItem.text

    property alias backgroundColor: backgroundRect.color
    property alias frameColor: backgroundRect.border.color
    property alias textColor: textItem.color

    property alias roundingRadius: backgroundRect.radius

    leftPadding: 24
    rightPadding: 24
    topPadding: 8
    bottomPadding: 8

    font.pixelSize: 22
    font.weight: Font.Light

    background: Rectangle
    {
        id: backgroundRect

        color: ColorTheme.colors.scene.banner.window
        border.color: ColorTheme.colors.scene.banner.frame
        radius: 2
    }

    contentItem: Text
    {
        id: textItem

        color: ColorTheme.colors.scene.banner.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}

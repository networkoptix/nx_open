// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx 1.0
import Nx.Controls 1.0

MenuItem
{
    id: menuItem

    height: 24

    leftPadding: 8
    rightPadding: 8

    contentItem: Text
    {
        id: label

        color: menuItem.hovered ? ColorTheme.colors.brand_contrast : ColorTheme.text
        opacity: menuItem.enabled ? 1.0 : 0.3
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        text: menuItem.text
    }
}

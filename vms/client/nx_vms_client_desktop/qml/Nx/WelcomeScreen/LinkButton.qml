// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core 1.0
import Nx.Controls 1.0
import nx.client.desktop 1.0

TextButton
{
    id: control

    leftPadding: 1
    rightPadding: 1
    topPadding: 0
    bottomPadding: 0

    color: ColorTheme.colors.dark17
    hoveredColor: ColorTheme.colors.light16
    pressedColor: color

    FocusFrame
    {
        anchors.fill: parent
        visible: control.pressed || control.focus
        color: ColorTheme.transparent(ColorTheme.highlight, 0.5)
    }
}

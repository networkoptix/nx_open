// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

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

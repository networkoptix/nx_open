// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core 1.0
import Nx.Controls.NavigationMenu 1.0

MenuItem
{
    property int level: 0
    property bool selected: false

    font.pixelSize: 14

    height: 28
    leftPadding: 23 + level * 8
    color:
    {
        const color = current || (selected && level === 0)
            ? ColorTheme.colors.light1
            : ColorTheme.colors.light10
        const opacity = active ? 1.0 : 0.3
        return ColorTheme.transparent(color, opacity)
    }
}

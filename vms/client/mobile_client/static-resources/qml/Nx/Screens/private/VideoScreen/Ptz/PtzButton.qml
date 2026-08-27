// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core
import Nx.Mobile.Controls

import Nx.Ui

Button
{
    id: control

    property bool overlayStyle: false

    foregroundColor: StyleHints.foregroundColor

    backgroundColor: overlayStyle
        ? (down
            ? ColorTheme.transparent(ColorTheme.colors.light1, 0.2)
            : ColorTheme.transparent(ColorTheme.colors.dark4, 0.5))
        : (down ? ColorTheme.colors.dark13 : ColorTheme.colors.dark11)

    borderColor: control.overlayStyle
        ? ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
        : "transparent"

    opacity: enabled ? 1 : 0.3
    radius: 0
    padding: 0
    icon.width: 24
    icon.height: 24
}

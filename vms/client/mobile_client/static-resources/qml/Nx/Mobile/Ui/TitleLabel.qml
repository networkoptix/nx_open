// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Text
{
    font.pixelSize: 18
    font.weight: 500
    color: ColorTheme.colors.light4
    elide: Text.ElideRight
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    // Workaround for QQuickText not recalculating implicitWidth on padding changes when eliding.
    onLeftPaddingChanged:
    {
        const mode = elide
        elide = Text.ElideNone
        elide = mode
    }
}

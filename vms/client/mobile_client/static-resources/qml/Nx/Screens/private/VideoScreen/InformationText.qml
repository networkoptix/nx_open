// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0

Text
{
    color: ColorTheme.transparent(ColorTheme.windowText, 0.8)
    font.pixelSize: 12
    width: Math.min(implicitWidth, 120)
    elide: Text.ElideRight
}

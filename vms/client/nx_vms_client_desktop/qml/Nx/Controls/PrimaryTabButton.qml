// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core

AbstractTabButton
{
    id: primaryTabButton

    leftPadding: 10
    rightPadding: 10

    topPadding: 10
    bottomPadding: 11

    topInset: 1
    bottomInset: 1

    backgroundColors
    {
        pressed: ColorTheme.colors.dark9
        hovered: ColorTheme.colors.dark11
    }

    underline.visible: primaryTabButton.isCurrent
    underline.height: 2
}

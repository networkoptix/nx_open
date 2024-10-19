// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

Menu
{
    id: control

    width:
    {
        let implicitItemWidth = 0
        for (let i = 0; i < control.count; ++i)
            implicitItemWidth = Math.max(implicitItemWidth, itemAt(i).implicitWidth)
        return implicitItemWidth + leftPadding + rightPadding
    }

    background: Rectangle { color: ColorTheme.colors.dark7 }

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside
    modal: true
    dim: false
    clip: true
}

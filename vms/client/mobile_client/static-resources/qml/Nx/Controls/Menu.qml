// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

Menu
{
    id: control

    property real minimumWidth: 200

    // The item to give the focus to once the menu is closed, instead of the item which had it
    // before the menu was opened. Reset after each closing.
    property Item itemToFocusAfterClose: null

    width:
    {
        let implicitItemWidth = 0
        for (let i = 0; i < control.count; ++i)
            implicitItemWidth = Math.max(implicitItemWidth, itemAt(i).implicitWidth)
        return Math.max(minimumWidth, implicitItemWidth + leftPadding + rightPadding)
    }

    topPadding: 8
    bottomPadding: 8

    background: Rectangle
    {
        color: ColorTheme.colors.dark14
        radius: 6
    }

    modal: true
    dim: false
    clip: true

    onClosed:
    {
        if (!itemToFocusAfterClose)
            return

        itemToFocusAfterClose.forceActiveFocus()
        itemToFocusAfterClose = null
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// IMPORTANT! Try to use PlatformMenu where possible.

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx 1.0
import Nx.Controls 1.0

Menu
{
    id: menu

    modal: true
    Overlay.modal: Item {}

    topPadding: 2
    bottomPadding: 2

    width: Math.max(implicitItemsWidth + leftPadding + rightPadding,
        background ? background.implicitWidth : 0)

    signal triggered(MenuItem item, var data)

    readonly property real implicitItemsWidth:
    {
        var result = 0

        for (var i = 0; i < count; ++i)
            result = Math.max(result, itemAt(i).implicitWidth)

        return result
    }

    delegate: MenuItem {}

    contentItem: Column
    {
        Repeater
        {
            model: menu.contentModel
        }
    }

    background: Rectangle
    {
        radius: 2
        color: ColorTheme.midlight
    }
}

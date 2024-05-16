// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Core 1.0
import Nx.Controls.NavigationMenu 1.0

NavigationMenu
{
    id: menu

    property var items: []

    implicitWidth: 240

    Repeater
    {
        id: lst
        model: items

        MenuItem
        {
            property int idx: index
            text: modelData
        }
    }

    function setCurrentIndex(idx)
    {
        if (idx >= 0 && idx < lst.count)
        {
            currentItemId = lst.itemAt(idx)
            itemClicked(currentItemId)
        }
    }

    function setIndexVisible(idx, visible)
    {
        if (idx >= 0 && idx < lst.count)
            lst.itemAt(idx).visible = visible
    }

    signal currentIndexChanged(int idx)
    onItemClicked: (item) => currentIndexChanged(item.idx)
}

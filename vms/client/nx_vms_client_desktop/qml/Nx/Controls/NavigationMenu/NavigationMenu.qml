// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx 1.0
import Nx.Controls 1.0
import Nx.Core 1.0

Scrollable
{
    id: control

    property var currentItemId: null
    default property alias data: column.data

    signal itemClicked(Item item)

    scrollView.topPadding: 16
    scrollView.bottomPadding: 16

    scrollView.background: Rectangle
    {
        color: ColorTheme.colors.dark8

        Rectangle
        {
            x: parent.width
            width: 1
            height: parent.height
            color: ColorTheme.shadow
        }
    }

    contentItem: Column
    {
        id: column
        width: control.width

        onChildrenChanged:
        {
            for (let i = 0; i < children.length; ++i)
            {
                let item = children[i]
                if (item.hasOwnProperty("navigationMenu"))
                    item.navigationMenu = control
            }
        }
    }
}

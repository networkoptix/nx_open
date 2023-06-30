// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Core 1.0

Rectangle
{
    id: panel

    enum Edge { Top, Bottom, Right, Left }
    property int edge: DialogPanel.Edge.Bottom

    color: ColorTheme.colors.dark7

    readonly property bool horizontal:
        edge === DialogPanel.Edge.Top || edge === DialogPanel.Edge.Bottom
    readonly property bool vertical: !horizontal

    implicitWidth: 60
    implicitHeight: 60

    anchors
    {
        left: (horizontal || edge === DialogPanel.Edge.Left) ? parent.left : undefined
        right: (horizontal || edge === DialogPanel.Edge.Right) ? parent.right : undefined
        top: (vertical || edge === DialogPanel.Edge.Top) ? parent.top : undefined
        bottom: (vertical || edge === DialogPanel.Edge.Bottom) ? parent.bottom : undefined
    }

    Item
    {
        transformOrigin:
        {
            if (edge === DialogPanel.Edge.Left)
                return Item.TopLeft
            if (edge === DialogPanel.Edge.Right)
                return Item.TopLeft
            return Item.Center
        }
        rotation:
        {
            if (edge === DialogPanel.Edge.Top)
                return 180
            if (edge === DialogPanel.Edge.Left)
                return 90
            if (edge === DialogPanel.Edge.Right)
                return -90
            return 0
        }

        width: horizontal ? parent.width : parent.height
        height: 1

        x: vertical && edge === DialogPanel.Edge.Left ? panel.width : 0
        y: vertical
           ? edge === DialogPanel.Edge.Left ? 0 : width
           : edge === DialogPanel.Edge.Top ? panel.height - 1 : 0

        Rectangle
        {
            width: parent.width
            height: 1
            color: ColorTheme.shadow
        }
        Rectangle
        {
            width: parent.width
            height: 1
            y: 1
            color: ColorTheme.dark
        }
    }
}

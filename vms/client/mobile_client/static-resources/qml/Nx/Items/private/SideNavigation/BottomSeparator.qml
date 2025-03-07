// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Nx.Core 1.0

Item
{
    implicitWidth: parent ? parent.width : 200
    implicitHeight: 8

    Rectangle
    {
        width: parent.width
        anchors.bottom: parent.top
        height: 4
        color: ColorTheme.transparent(ColorTheme.colors.dark4, 0.2)
    }

    Rectangle
    {
        width: parent.width
        height: 1
        color: ColorTheme.colors.dark9
    }
}

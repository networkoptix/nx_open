// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Nx.Core 1.0

SideNavigationItem
{
    id: control

    property string icon: ""
    property string text: ""

    contentItem: Item
    {
        Image
        {
            anchors.verticalCenter: parent.verticalCenter
            x: 16
            source: control.icon
        }

        Text
        {
            x: 56
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: ColorTheme.windowText
        }
    }
}

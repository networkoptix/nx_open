// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0

Item
{
    implicitWidth: parent ? parent.width : 100
    implicitHeight: 2

    Rectangle
    {
        id: baseline

        width: parent.width
        height: 1
        anchors.centerIn: parent
        color: ColorTheme.colors.dark9
    }

    Rectangle
    {
        y: baseline.y - 1
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width
        height: 1
        color: ColorTheme.colors.dark5
    }
}

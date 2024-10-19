// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

MenuSeparator
{
    id: control

    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    contentItem: Item
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
}

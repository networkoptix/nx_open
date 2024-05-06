// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

CompactTabButton
{
    id: tabButton

    readonly property var tabBar: TabBar.tabBar

    font.capitalization: Font.AllUppercase

    compactWidth: 35
    normalWidth: tabBar ? (tabBar.width - (TabBar.tabBar.count - 1) * compactWidth) : 0

    Rectangle
    {
        anchors.fill: parent
        anchors.rightMargin: -1
        color: "transparent"
        border.color: tabBar ? tabBar.bottomBorderColor : "transparent"
        z: -10
    }
}

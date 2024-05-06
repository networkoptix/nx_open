// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Rectangle
{
    id: unreadCounter

    property int count: 0

    radius: 6
    width: radius * 2
    height: radius * 2
    visible: count > 0
    border.color: ColorTheme.colors.dark3

    Text
    {
        id: text

        anchors.centerIn: unreadCounter
        leftPadding: 1

        color: unreadCounter.border.color
        text: unreadCounter.count > 99 ? "99+" : unreadCounter.count

        font.pixelSize: 8
        font.bold: true
    }
}

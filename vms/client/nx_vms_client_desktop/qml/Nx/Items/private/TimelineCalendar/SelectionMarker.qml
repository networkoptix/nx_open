// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx

Item
{
    id: marker

    property bool start: false
    property bool end: false
    readonly property bool middle: !start && !end

    readonly property real radius: middle ? 0 : 4

    Rectangle
    {
        height: parent.height
        width: parent.width - marker.radius
        x: end ? 0 : marker.radius
        color: ColorTheme.colors.dark7
    }

    Rectangle
    {
        anchors.fill: parent
        radius: marker.radius
        color: ColorTheme.colors.dark11
        visible: !middle
    }

    layer.enabled: !middle
}

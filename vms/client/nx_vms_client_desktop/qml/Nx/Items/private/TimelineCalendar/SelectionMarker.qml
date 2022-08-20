// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx 1.0

Rectangle
{
    property bool start: false
    property bool end: false

    readonly property bool middle: !start && !end

    radius: middle ? 0 : 2
    color: middle ? ColorTheme.colors.dark9 : ColorTheme.colors.dark13

    layer.enabled: straightCornersRectangle.visible

    Rectangle
    {
        id: straightCornersRectangle
        width: parent.radius
        height: parent.height
        x: start ? parent.width - width : 0
        visible: start !== end
        color: parent.color
    }
}

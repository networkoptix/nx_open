// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

Rectangle
{
    id: item
    property point centerPoint: Qt.point(0, 0)
    property color borderColor: circleColor
    property alias circleColor: item.color

    color: "transparent"
    border.width: 0
    border.color: borderColor

    x: centerPoint.x - width / 2
    y: centerPoint.y - height / 2
    width: radius * 2
    height: width
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import nx.vms.client.core

Rectangle
{
    id: line

    property real startX
    property real startY
    property real endX
    property real endY
    property alias lineWidth: line.height

    antialiasing: true

    x: startX
    y: startY - height / 2
    width: Geometry.length(Qt.point(endX - startX, endY - startY))

    transformOrigin: Item.Left
    rotation: Math.atan2(endY - startY, endX - startX) / Math.PI * 180
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Core 1.0

Rectangle
{
    property var originJson
    property var sizeJson
    property real defaultSize: 0
    property color baseColor: "red"

    property real scaleX: parent ? parent.width : 1
    property real scaleY: parent ? parent.height : 1

    color: ColorTheme.transparent(baseColor, 0.3)
    border.color: baseColor
    border.width: 2

    readonly property point defaultOrigin: Qt.point((1 - size.width) / 2, (1 - size.height) / 2)

    readonly property point origin: Array.isArray(originJson)
        ? Qt.point(
            CoreUtils.getValue(originJson[0], defaultOrigin.x),
            CoreUtils.getValue(originJson[1], defaultOrigin.y))
        : defaultOrigin

    readonly property size size: Array.isArray(sizeJson)
        ? Qt.size(
            CoreUtils.getValue(sizeJson[0], defaultSize),
            CoreUtils.getValue(sizeJson[1], defaultSize))
        : Qt.size(defaultSize, defaultSize)

    x: origin.x * scaleX
    y: origin.y * scaleY
    width: Math.max(size.width * scaleX, border.width)
    height: Math.max(size.height * scaleY, border.width)
}

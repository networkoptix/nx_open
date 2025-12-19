// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

Slider
{
    id: slider

    property bool fillingUp: false
    property real implicitLength: 100

    property real pageSize: stepSize * 10
    stepSize: (to - from) / 100

    property real handleSize: 14
    property real grooveWidth: 2

    property color grooveColor: ColorTheme.colors.dark11
    property color grooveBorderColor: "transparent"

    property color filledGrooveColor: handleBorderColor

    property color handleColor: pressed ? ColorTheme.colors.dark13 : ColorTheme.colors.dark7

    property color handleBorderColor: pressed
        ? ColorTheme.colors.light4
        : ColorTheme.colors.light10

    property real handleBorderWidth: 2

    padding: 2

    background: Rectangle
    {
        id: groove

        readonly property real borders: border.width * 2

        readonly property real effectiveGroveWidth:
            Math.min(slider.grooveWidth, slider.handleSize + borders)

        implicitWidth: slider.horizontal ? implicitLength : effectiveGroveWidth
        implicitHeight: slider.horizontal ? effectiveGroveWidth : implicitLength

        width: slider.horizontal ? slider.availableWidth : implicitWidth
        height: slider.horizontal ? implicitHeight : slider.availableHeight

        x: slider.leftPadding + (slider.horizontal ? 0 : ((slider.availableWidth - width) * 0.5))
        y: slider.topPadding + (slider.horizontal ? ((slider.availableHeight - height) * 0.5) : 0)

        radius: grooveWidth / 2

        color: slider.grooveColor
        border.color: slider.grooveBorderColor

        Rectangle
        {
            id: grooveHighlight

            visible: slider.fillingUp

            x: groove.border.width

            y: slider.horizontal
                ? groove.border.width
                : (handle.y - groove.y)

            width: slider.horizontal
                ? (handle.x + handle.width - groove.x - 1)
                : (groove.width - groove.borders)

            height: groove.height - groove.border.width - y
            radius: groove.radius - groove.borders

            color: slider.filledGrooveColor
        }
    }

    handle: Rectangle
    {
        x: slider.leftPadding + (slider.horizontal
            ? slider.visualPosition * (slider.availableWidth - width)
            : 0)

        y: slider.topPadding + (slider.horizontal
            ? 0
            : slider.visualPosition * (slider.availableHeight - height))

        implicitWidth: slider.handleSize
        implicitHeight: slider.handleSize
        radius: slider.handleSize / 2

        color: slider.handleColor

        border.width: slider.handleBorderWidth
        border.color: slider.handleBorderColor
    }
}

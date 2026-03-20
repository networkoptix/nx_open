// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as QtQuickControls

import Nx.Core

QtQuickControls.Slider
{
    id: slider

    enum GrooveHighlight
    {
        NoGrooveHighlight,
        FillingFromBottom,
        FillingFromZero
    }
    property int grooveHighlight: Slider.NoGrooveHighlight

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
        border.width: (slider.grooveBorderColor.a > 0.01) ? 1 : 0

        Rectangle
        {
            id: grooveHighlightRect

            readonly property rect bounds:
            {
                switch (slider.grooveHighlight)
                {
                    case Slider.FillingFromBottom:
                    {
                        if (slider.horizontal)
                        {
                            return Qt.rect(
                                groove.border.width,
                                groove.border.width,
                                handle.x + handle.width - groove.x - 1,
                                groove.height - groove.borders)
                        }

                        return Qt.rect(
                            groove.border.width,
                            handle.y - groove.y,
                            groove.width - groove.borders,
                            groove.height - groove.border.width - (handle.y - groove.y))
                    }

                    case Slider.FillingFromZero:
                    {
                        const from = slider.from < slider.to ? slider.from : slider.to
                        const to = slider.from < slider.to ? slider.to : slider.from

                        const range = to - from
                        if (range < 0.0001)
                            return Qt.rect(0, 0, 0, 0)

                        const zero = MathUtils.bound(from, 0, to)
                        const value = MathUtils.bound(from, slider.value, to) //< Just in case.
                        const start = value > zero ? zero : slider.value
                        const pos = (start - from) / range
                        const length = Math.abs(value - zero) / range

                        if (slider.horizontal)
                        {
                            const grooveLength = groove.width - groove.borders
                            return Qt.rect(
                                groove.border.width + pos * grooveLength,
                                groove.border.width,
                                length * grooveLength,
                                groove.height - groove.borders)
                        }

                        const grooveLength = groove.height - groove.borders
                        return Qt.rect(
                            groove.border.width,
                            groove.height - groove.border.width
                                - (pos + length) * grooveLength,
                            groove.width - groove.borders,
                            length * grooveLength)
                    }

                    case Slider.NoGrooveHighlight:
                    default:
                    {
                        return Qt.rect(0, 0, 0, 0)
                    }
                }
            }

            x: bounds.left
            y: bounds.top
            width: bounds.width
            height: bounds.height
            radius: groove.radius - groove.borders
            color: slider.filledGrooveColor
            visible: slider.grooveHighlight !== Slider.NoGrooveHighlight
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

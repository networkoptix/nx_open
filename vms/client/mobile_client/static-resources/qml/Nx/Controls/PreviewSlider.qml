// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items

Slider
{
    id: slider

    required property IntervalPreview preview
    property bool playerWasPlaying: false
    readonly property real handlePosition:
        visualPosition * (width - leftPadding - rightPadding) + leftPadding - handle.width / 2

    height: 36

    leftPadding: 12
    rightPadding: 12

    from: preview.startTimeMs
    to: preview.startTimeMs + preview.durationMs

    handle: Rectangle
    {
        id: handle
        width: 24
        height: 24

        x: slider.handlePosition
        anchors.verticalCenter: slider.verticalCenter

        radius: 12
        color: ColorTheme.colors.dark14

        border.width: 4
        border.color: ColorTheme.colors.light4
    }

    background: Rectangle
    {
        id: sliderBackground

        width: parent.width
        height: 4
        color: ColorTheme.colors.dark14
        anchors.verticalCenter: slider.verticalCenter

        Rectangle
        {
            width: slider.handlePosition
            height: parent.height
            color: ColorTheme.colors.brand
        }
    }

    onValueChanged:
    {
        if (!pressed && value == to && !preview.autoRepeat)
            preview.preview()
    }

    onPressedChanged:
    {
        if (pressed)
        {
            playerWasPlaying = preview.playingPlayerState
            preview.preview()
        }
        else if (playerWasPlaying && value != to)
        {
            CoreUtils.executeLater(function()
            {
                preview.play(preview.position)
            }, this)
        }
    }

    Connections
    {
        target: slider
        enabled: slider.pressed

        function onValueChanged()
        {
            preview.setPosition(slider.value)
        }
    }

    Binding
    {
        target: slider
        when: !slider.pressed
        property: "value"
        value: preview.atEnd ? slider.to : preview.position
    }
}

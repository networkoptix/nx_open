// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

import nx.vms.client.desktop

Slider
{
    id: slider

    property bool fillingUp: false
    property real implicitLength: 100

    property real pageSize: stepSize * 10
    stepSize: (to - from) / 100

    property real handleSize: 16
    property real grooveWidth: 4

    padding: 2
    focusPolicy: Qt.TabFocus

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

        color: slider.hovered ? ColorTheme.colors.dark12 : ColorTheme.colors.dark11
        border.color: ColorTheme.colors.dark5

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

            color: enabled
                ? (slider.hovered ? ColorTheme.colors.light15 : ColorTheme.colors.light16)
                : ColorTheme.colors.dark15
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

        color: slider.pressed ? ColorTheme.colors.dark10 : ColorTheme.colors.dark7

        border.width: 2
        border.color:
        {
            if (!enabled)
                return ColorTheme.colors.dark15

            return (hoverHandler.hovered || slider.pressed)
                ? ColorTheme.colors.light12
                : ColorTheme.colors.light16
        }

        HoverHandler { id: hoverHandler }
    }

    FocusFrame
    {
        id: focusFrame

        anchors.fill: parent
        visible: slider.activeFocus
        color: ColorTheme.transparent(ColorTheme.highlight, 0.5)
        frameWidth: 1
    }

    Keys.onPressed:
    {
        event.accepted = true
        switch (event.key)
        {
            case Qt.Key_Home:
                value = from
                break

            case Qt.Key_End:
                value = to
                break

            case Qt.Key_PageDown:
                value -= pageSize
                break

            case Qt.Key_PageUp:
                value += pageSize
                break

            case Qt.Key_Left:
            case Qt.Key_Down:
                decrease()
                break;

            case Qt.Key_Right:
            case Qt.Key_Up:
                increase()
                break;

            default:
                event.accepted = false
                break
        }

        if (event.accepted)
            moved()
    }

    WheelHandler
    {
        onWheel:
        {
            const kSensitivity = 1.0 / 120.0
            slider.value += slider.stepSize * event.angleDelta.y * kSensitivity
            event.accepted = true
            moved()
        }
    }
}

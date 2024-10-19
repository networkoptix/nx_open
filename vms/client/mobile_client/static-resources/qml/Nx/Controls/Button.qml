// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4
import Nx.Core 1.0
import nx.vms.client.core 1.0

AbstractButton
{
    id: control

    property bool flat: false
    property color color: ColorTheme.colors.dark9
    property color hoveredColor: Qt.lighter(color)
    property color textColor: ColorTheme.windowText
    property color pressedTextColor: textColor
    property color highlightColor: "#30ffffff"
    property real radius: 2
    property real labelPadding: text ? 24 : 0

    property alias mouseX: mouseTracker.mouseX
    property alias mouseY: mouseTracker.mouseY

    implicitWidth: Math.max(
        background ? background.implicitWidth : 0,
        contentItem ? contentItem.implicitWidth : 0) + leftPadding + rightPadding
    implicitHeight: Math.max(
        background ? background.implicitHeight : 0,
        contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding

    padding: 6
    leftPadding: 8
    rightPadding: 8

    font.pixelSize: 16
    font.weight: Font.DemiBold

    icon.color: "transparent"

    onPressAndHold: { d.pressedAndHeld = true }
    onCanceled: { d.pressedAndHeld = false }
    onReleased:
    {
        if (!d.pressedAndHeld)
            return

        d.pressedAndHeld = false
        clicked()
    }

    contentItem: IconLabel
    {
        leftPadding: control.labelPadding
        rightPadding: control.labelPadding
        spacing: 8
        icon: control.icon
        text: control.text
        font: control.font
        color: control.pressed
            ? control.pressedTextColor
            : control.textColor
    }

    background: Rectangle
    {
        implicitWidth: 64
        implicitHeight: 40
        radius: control.radius

        width: parent.availableWidth
        height: parent.availableHeight
        x: parent.leftPadding
        y: parent.topPadding

        color:
        {
            if (control.flat)
                return "transparent"

            if (!control.enabled)
                return control.color

            if (control.highlighted)
                return Qt.lighter(control.color)

            if (control.hovered)
                return control.hoveredColor

            return control.color
        }
        opacity: control.enabled ? 1.0 : 0.3

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            radius: parent.radius
            mouseArea: control
            rippleSize: 160
            highlightColor: control.highlightColor
        }
    }

    MouseTracker
    {
        id: mouseTracker

        item: control
        hoverEventsEnabled: true
    }

    QtObject
    {
        id: d

        property bool pressedAndHeld: false
    }
}

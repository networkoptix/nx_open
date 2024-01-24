// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

import nx.vms.client.core

Button
{
    id: control

    property url iconUrl
    property url checkedIconUrl: iconUrl
    property color backgroundColor: ColorTheme.transparent(hoverColor, 0)
    property color hoverColor: ColorTheme.colors.brand_core
    property color pressedColor: ColorTheme.darker(ColorTheme.colors.brand_core, 3)
    property color checkedColor: ColorTheme.darker(ColorTheme.colors.brand_core, 1)

    readonly property alias mousePosition: mouseTracker.position

    leftPadding: 1
    rightPadding: 1
    topPadding: 1
    bottomPadding: 1

    implicitWidth: 26
    implicitHeight: 26

    height: parent ? Math.min(implicitHeight, parent.height) : implicitHeight
    width: height

    background: Rectangle
    {
        color:
        {
            if (control.pressed)
                return pressedColor

            if (mouseTracker.containsMouse)
                return hoverColor

            if (control.checked)
                return checkedColor

            return backgroundColor
        }
        Behavior on color { ColorAnimation { duration: 50 } }
    }

    contentItem: Image
    {
        anchors.centerIn: control

        width: control.availableWidth
        height: control.availableHeight

        source: control.checked ? checkedIconUrl : iconUrl
    }

    MouseTracker
    {
        id: mouseTracker
        item: control
        hoverEventsEnabled: true
    }
}

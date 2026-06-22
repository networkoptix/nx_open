// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

/**
 * This component dims an area and makes it unresponsive to mouse or touch events.
 */
Rectangle
{
    id: dimmer

    property bool active: false

    color: ColorTheme.transparent(ColorTheme.colors.dark4, 0.7)
    opacity: active ? 1 : 0
    visible: opacity > 0.001

    Behavior on opacity { NumberAnimation { duration: 150 }}

    WheelHandler
    {
        enabled: dimmer.active
        grabPermissions: PointerHandler.TakeOverForbidden
    }

    MultiPointTouchArea
    {
        enabled: dimmer.active
        anchors.fill: dimmer
    }

    TapHandler {}
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

import Nx.Core

/**
 * Standard Popup means allow to dim the whole window under the popup.
 * This component allows to dim only some rectangle of the window, and make it not responsive to
 * mouse/touch events.
 *
 * CustomPopupDimmer should be positioned where needed, and the linked popup should be specified
 * in the `popup` property. The dimmer will be automatically shown and hidden with the popup.
 */
Rectangle
{
    id: dimmer

    required property T.Popup popup
    readonly property alias opened: d.opened

    color: ColorTheme.transparent(ColorTheme.colors.dark4, 0.7)
    opacity: d.opened ? 1 : 0
    visible: opacity > 0.001
    z: 10000 //< Something really big.

    Behavior on opacity { NumberAnimation { duration: 150 }}

    onPopupChanged:
        d.opened = !!popup?.opened

    Connections
    {
        id: d

        property bool opened: false

        target: dimmer.popup

        function onAboutToShow()
        {
            d.opened = true
        }

        function onAboutToHide()
        {
            d.opened = false
        }
    }

    WheelHandler
    {
        enabled: d.opened
        grabPermissions: PointerHandler.TakeOverForbidden
    }

    MultiPointTouchArea
    {
        enabled: d.opened
        anchors.fill: dimmer
    }

    TapHandler {}
}

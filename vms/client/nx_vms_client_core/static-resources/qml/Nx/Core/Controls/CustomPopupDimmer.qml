// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

/**
 * Standard Popup means allow to dim the whole window under the popup.
 * This component allows to dim only some rectangle of the window, and make it not responsive to
 * mouse/touch events.
 *
 * CustomPopupDimmer should be positioned where needed, and the linked popup should be specified
 * in the `popup` property. The dimmer will be automatically shown and hidden with the popup.
 */
Dimmer
{
    id: dimmer

    required property T.Popup popup
    readonly property alias opened: d.opened

    active: opened

    z: 10000 //< Something really big.

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
}

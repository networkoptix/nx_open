// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import nx.vms.client.desktop

Popup
{
    id: popup

    property bool closeOnParentClosing:
        !([Popup.NoAutoClose, Popup.CloseOnEscape].includes(popup.closePolicy))

    property bool closeOnOtherWindowClick:
        !([Popup.NoAutoClose, Popup.CloseOnEscape].includes(popup.closePolicy))

    Connections
    {
        target: popup.parent
        enabled: popup.closeOnParentClosing && target && popup.visible

        function onVisibleChanged()
        {
            if (!target.visible)
                popup.close()
        }
    }

    Connections
    {
        target: popup.parent && popup.parent.Window.window
        enabled: popup.closeOnParentClosing && target && popup.visible

        function onVisibleChanged()
        {
            if (!target.visible)
                popup.close()
        }
    }

    MouseSpy.onMousePress: (mouse) =>
    {
        if (!popup.visible || !popup.closeOnOtherWindowClick)
            return

        const overlay = popup.Overlay.overlay
        const pt = overlay.mapFromGlobal(mouse.globalPosition)

        if (pt.x < 0 || pt.y < 0 || pt.x >= overlay.width || pt.y >= overlay.height)
            popup.close()
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

/**
 * Highlighting rectangle automatically binds to the focused item in the window. Can be placed
 * anywhere (parent changes but the initial ownership is preserved).
 */
DebugRect
{
    border.color: "cyan"

    Window.onActiveFocusItemChanged:
    {
        if (Window.activeFocusItem)
            parent = Window.activeFocusItem
    }
}

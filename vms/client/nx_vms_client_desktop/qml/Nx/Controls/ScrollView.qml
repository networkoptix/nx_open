// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

import Nx.Controls 1.0

ScrollView
{
    id: control

    ScrollBar.vertical: ScrollBar
    {
        parent: control
        x: control.mirrored ? 0 : control.width - width
        y: control.topPadding
        height: control.availableHeight
    }

    ScrollBar.horizontal: ScrollBar
    {
        parent: control
        x: control.leftPadding
        y: control.height - height
        width: control.availableWidth
    }

    // QTBUG-74230 workaround.

    function hideExtraScrollbars()
    {
        for (let i = 0; i < children.length; ++i)
        {
            let child = control.children[i]
            if ((child instanceof T.ScrollBar)
                && child !== control.ScrollBar.vertical
                && child !== control.ScrollBar.horizontal)
            {
                child.active = false
                child.visible = false
            }
        }
    }

    Component.onCompleted:
        hideExtraScrollbars()

    ScrollBar.onVerticalChanged:
        hideExtraScrollbars()

    ScrollBar.onHorizontalChanged:
        hideExtraScrollbars()
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0

NxObject
{
    id: autoScroller

    /** Scroll bar to control. */
    property var scrollBar

    /** Required scroll velocity (signed) in pixels/ms. */
    property real velocity: 0

    /** Scrollable content size in pixels. */
    property real contentSize: 100

    NumberAnimation
    {
        id: animation

        target: autoScroller.scrollBar
        property: "position"

        function update()
        {
            stop()
            if (!scrollBar || contentSize <= 0 || Math.abs(velocity) < 0.01)
                return

            from = scrollBar.position
            to = (velocity < 0) ? 0 : (1.0 - scrollBar.size)
            duration = contentSize * Math.abs(to - from) / Math.abs(velocity)
            start()
        }
    }

    onVelocityChanged:
        animation.update()

    onScrollBarChanged:
        animation.update()

    onContentSizeChanged:
        animation.update()
}

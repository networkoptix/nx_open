// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core

LayoutItemProxy
{
    property alias background: proxyBackground

    clip: true

    onTargetChanged:
    {
        for (let i of children)
        {
            // When the proxy target is changed to another one, parent is not restored for the old
            // target item. It leads to appearing old item under the new target.
            // Reset item's parent to prevent it appearing on the background.
            // TODO: Save target initial parent and restore it when the item is not a current target
            // anymore.
            if (i !== proxyBackground && i !== target)
                i.parent = null
        }
    }

    Rectangle
    {
        id: proxyBackground

        color: "transparent"
        anchors.fill: parent
    }
}

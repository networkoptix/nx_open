// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

// Tightly wraps the provided single line Text item from the baseline to the capital height.
Item
{
    id: wrapper

    required property Text textItem

    implicitWidth: textItem.width
    implicitHeight: metrics.capitalHeight

    Binding
    {
        wrapper.textItem
        {
            parent: wrapper
            y: metrics.capitalHeight - wrapper.textItem.baselineOffset
            wrapMode: Text.NoWrap
        }
    }

    FontMetrics
    {
        id: metrics
        font: wrapper.textItem.font
    }
}

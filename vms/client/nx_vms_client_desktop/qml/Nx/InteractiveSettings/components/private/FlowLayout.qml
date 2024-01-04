// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

/**
 * Flow layout with baseline alignment.
 */
LayoutBase
{
    id: layout

    property bool ready: false

    baselineOffset: ready && layoutItems.length > 0
        ? Math.max(...Array.from(layoutItems).map(item => item.baselineOffset))
        : 0

    contentItem: Flow
    {
        id: flow

        spacing: 8

        Repeater
        {
            model: layoutItems

            onItemAdded: (index) => layout.ready = index === count - 1

            Control
            {
                contentItem: modelData

                // Baseline alignment.
                topPadding: contentItem.baselineOffset > 0
                    ? layout.baselineOffset - contentItem.baselineOffset
                    : 0

                // Force parent change (for items with already set parent).
                onContentItemChanged: contentItem.parent = this
            }
        }
    }
}

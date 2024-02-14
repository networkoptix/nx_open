// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import "private"

/**
 * Interactive Settings type.
 */
LabeledItem
{
    property alias childrenItem: layout
    property alias filledCheckItems: group.filledCheckItems
    property alias filled: group.filled

    contentItem: Group
    {
        id: group

        baselineOffset: layout.baselineOffset
        childrenItem: layout

        contentItem: FlowLayout
        {
            id: layout
        }
    }

    function updateFilled() { group.updateFilled() }
}

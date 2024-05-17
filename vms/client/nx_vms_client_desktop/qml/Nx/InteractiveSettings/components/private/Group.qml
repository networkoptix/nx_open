// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import "utils.js" as Utils

/**
 * Interactive Settings private type.
 */
Item
{
    property var filledCheckItems: undefined

    property Item contentItem: null
    property Item childrenItem: null

    implicitWidth: contentItem ? contentItem.implicitWidth : 0
    implicitHeight: contentItem ? contentItem.implicitHeight : 0

    property bool filled: false

    onContentItemChanged:
    {
        contentItem.parent = this
        contentItem.anchors.fill = this
        contentItem.anchors.margins = 0
    }

    function getItemsToCheck()
    {
        if (!childrenItem)
            return []

        if (!Array.isArray(filledCheckItems))
            return childrenItem.layoutItems

        let result = []

        for (let i = 0; i < childrenItem.layoutItems.length; ++i)
        {
            const item = childrenItem.layoutItems[i]
            if (item.hasOwnProperty("name") && filledCheckItems.indexOf(item.name) >= 0)
                result.push(item)
        }

        return result
    }

    function updateFilled()
    {
        const itemsToCheck = getItemsToCheck()

        if (itemsToCheck.length === 0)
        {
            filled = true
            return
        }

        for (let i = 0; i < itemsToCheck.length; ++i)
        {
            if (Utils.isItemFilled(itemsToCheck[i]))
            {
                filled = true
                return
            }
        }

        filled = false
    }
}

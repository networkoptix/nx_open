import QtQuick 2.0

import "utils.js" as Utils

Item
{
    property string filledCheckPolicy: "anyOfItems"
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

    Connections
    {
        target: childrenItem
        onChildrenChanged: updateFilled()
    }

    function processFilledChanged()
    {
        updateFilled()
    }

    function getItemsToCheck()
    {
        if (!childrenItem)
            return []

        if (!Array.isArray(filledCheckItems))
            return childrenItem.children

        var result = []

        for (var i = 0; i < childrenItem.children.length; ++i)
        {
            const item = childrenItem.children[i]
            if (item.hasOwnProperty("name") && filledCheckItems.indexOf(item.name) >= 0)
                result.push(item)
        }

        return result
    }

    function isAnyOfItemsFilled()
    {
        const itemsToCheck = getItemsToCheck()

        for (var i = 0; i < itemsToCheck.length; ++i)
        {
            if (Utils.isItemFilled(itemsToCheck[i]))
                return true
        }

        return false
    }

    function areAllItemsFilled()
    {
        const itemsToCheck = getItemsToCheck()

        for (var i = 0; i < itemsToCheck.length; ++i)
        {
            if (!Utils.isItemFilled(itemsToCheck[i]))
                return false
        }

        return true
    }

    function updateFilled()
    {
        if (!contentItem)
        {
            filled = false
            return
        }

        if (filledCheckPolicy === "anyOfItems")
            filled = isAnyOfItemsFilled()
        else if (filledCheckPolicy === "allItems")
            filled = areAllItemsFilled()
        else
            filled = false
    }
}

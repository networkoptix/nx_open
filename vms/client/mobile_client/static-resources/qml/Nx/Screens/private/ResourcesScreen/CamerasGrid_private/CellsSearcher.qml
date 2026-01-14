// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    required property var repeater
    required property var flickable
    required property int spacing

    function findCellClosestToViewportCenter()
    {
        const viewportCenterY = flickable.contentY + flickable.height / 2

        let closestCellIndex = -1
        let minDistance = Number.MAX_VALUE

        for (let i = 0; i < repeater.count; ++i)
        {
            const cellItem = repeater.itemAt(i)
            if (!cellItem)
            {
                if (minDistance === Number.MAX_VALUE)
                    continue //< Still haven't found any loaded item.
                else
                    break //< No need to check the rest of items - they all unloaded.
            }

            const cellCenterY = cellItem.y + cellItem.height / 2
            const distance = Math.abs(cellCenterY - viewportCenterY)

            if (distance < minDistance)
            {
                minDistance = distance
                closestCellIndex = i
            }
        }

        return closestCellIndex
    }

    function findClosestFullyVisibleCell(scenePoint)
    {
        const flickablePoint = flickable.mapFromItem(null, scenePoint.x, scenePoint.y)
        const contentPoint = Qt.point(
            flickablePoint.x,
            flickablePoint.y + flickable.contentY)

        let enclosingIndex = -1
        let nearestIndex = repeater.count - 1
        let minDistance = Number.MAX_VALUE

        for (let i = 0; i < repeater.count; ++i)
        {
            const cellItem = repeater.itemAt(i)
            if (!cellItem)
                continue

            if (!d.cellIsFullyVisible(cellItem))
                continue

            if (contentPoint.x >= cellItem.x
                && contentPoint.x < cellItem.x + cellItem.width + spacing
                && contentPoint.y >= cellItem.y
                && contentPoint.y < cellItem.y + cellItem.height + spacing)
            {
                enclosingIndex = i
                break
            }

            const cellCenter =
                Qt.point(cellItem.x + cellItem.width / 2, cellItem.y + cellItem.height / 2)
            const distance = MathUtils.distance(contentPoint, cellCenter)
            if (distance < minDistance)
            {
                minDistance = distance
                nearestIndex = i
            }
        }

        return enclosingIndex !== -1 ? enclosingIndex : nearestIndex
    }

    NxObject
    {
        id: d

        function cellIsFullyVisible(cellItem)
        {
            return cellItem.y >= flickable.contentY
                && (cellItem.y + cellItem.height) <= (flickable.contentY + flickable.height)
        }
    }
}

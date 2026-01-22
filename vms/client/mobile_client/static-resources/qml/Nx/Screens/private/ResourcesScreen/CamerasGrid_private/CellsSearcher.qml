// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    required property var sizesCalculator
    required property var geometryCalculator

    function findCellClosestToViewportCenter(contentY, flickableWidth, flickableHeight)
    {
        const viewportCenter = Qt.point(flickableWidth / 2, contentY + flickableHeight / 2)

        let closestCellIndex = -1
        let minDistance = Number.MAX_VALUE

        for (let i = 0; i < sizesCalculator.cellsCount; ++i)
        {
            const cellGeometry = geometryCalculator.calculateCellGeometry(i)

            const cellCenter = Qt.point(
                cellGeometry.x + cellGeometry.width / 2,
                cellGeometry.y + cellGeometry.height / 2)

            const distance = MathUtils.distance(viewportCenter, cellCenter)

            if (distance < minDistance)
            {
                minDistance = distance
                closestCellIndex = i
            }
        }

        return closestCellIndex
    }

    function findClosestFullyVisibleCell(flickablePoint, contentY, flickableHeight)
    {
        const contentPoint = Qt.point(
            flickablePoint.x,
            flickablePoint.y + contentY)

        let enclosingIndex = -1
        let nearestIndex = sizesCalculator.cellsCount - 1
        let minDistance = Number.MAX_VALUE

        for (let i = 0; i < sizesCalculator.cellsCount; ++i)
        {
            const cellGeometry = geometryCalculator.calculateCellGeometry(i)

            if (!d.cellIsFullyVisible(cellGeometry, contentY, flickableHeight))
                continue

            if (contentPoint.x >= cellGeometry.x
                && contentPoint.x < cellGeometry.x + cellGeometry.width + sizesCalculator.spacing
                && contentPoint.y >= cellGeometry.y
                && contentPoint.y < cellGeometry.y + cellGeometry.height + sizesCalculator.spacing)
            {
                enclosingIndex = i
                break
            }

            const cellCenter = Qt.point(
                cellGeometry.x + cellGeometry.width / 2, cellGeometry.y + cellGeometry.height / 2)
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

        function cellIsFullyVisible(cellGeometry, contentY, flickableHeight)
        {
            return cellGeometry.y >= contentY
                && (cellGeometry.y + cellGeometry.height) <= (contentY + flickableHeight)
        }
    }
}

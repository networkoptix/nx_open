// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    // Input properties:
    property int cellsCount: 0
    property real availableWidth: 0.0
    property real availableHeight: 0.0
    property real spacing: 0.0

    // Output properties:
    readonly property int defaultColumnsCount: d.calculateDefaultColumnsCount()
    readonly property int columnsCount: d.userDefinedColumnsCount !== kInvalidColumnsCount
        ? MathUtils.bound(1, d.userDefinedColumnsCount, defaultColumnsCount)
        : defaultColumnsCount

    readonly property int normalCellWidth: d.calculateNormalCellWidth()
    readonly property int enlargedCellWidth: d.calculateEnlargedCellWidth()
    readonly property int enlargedCellsCount: d.calculateEnlargedCellsCount()
    readonly property int enlargedRowsCount:
    {
        if (enlargedCellsCount === 0)
            return 0
        return enlargedCellsCount < columnsCount ? 1 : 2
    }
    readonly property int enlargedCellsCountInFirstRow: enlargedRowsCount > 0
        ? Math.floor(enlargedCellsCount / enlargedRowsCount)
        : 0

    // Constants:
    readonly property int kInvalidColumnsCount: -1
    readonly property real kAspectRatio: kMinCellHeight / kMinCellWidth
    readonly property int kMinCellWidth: 160
    readonly property int kMinCellHeight: 90

    function setUserDefinedColumnsCount(newColumnsCount)
    {
        if (newColumnsCount === defaultColumnsCount)
            d.userDefinedColumnsCount = kInvalidColumnsCount
        else if (newColumnsCount >= 1 && newColumnsCount < defaultColumnsCount)
            d.userDefinedColumnsCount = newColumnsCount
        else //< Display size changed.
            d.userDefinedColumnsCount = kInvalidColumnsCount
    }

    readonly property int totalLayoutHeight:
    {
        const enlargedRowsTotalHeight =
            enlargedRowsCount * (calculateCellHeight(enlargedCellWidth) + spacing) - spacing

        const normalRowsCount = Math.ceil((cellsCount - enlargedCellsCount) / columnsCount)
        const normalRowsTotalHeight =
            normalRowsCount * (calculateCellHeight(normalCellWidth) + spacing) - spacing

        return enlargedRowsTotalHeight + normalRowsTotalHeight
            + ((enlargedRowsCount > 0 && normalRowsCount > 0) ? spacing : 0)
    }

    function getUserDefinedColumnsCount()
    {
        if (d.userDefinedColumnsCount === kInvalidColumnsCount)
            return defaultColumnsCount
        else
            return d.userDefinedColumnsCount
    }

    function calculateCellHeight(cellWidth)
    {
        return Math.ceil(cellWidth * kAspectRatio)
    }

    function getCellsCountToReorder()
    {
        const notFullRowSize = cellsCount % columnsCount
        if (notFullRowSize === 0)
            return 0

        const thereIsNoHorizontalSpaceForCellsReordering = notFullRowSize === columnsCount - 1
        if (thereIsNoHorizontalSpaceForCellsReordering)
            return notFullRowSize

        return columnsCount + notFullRowSize
    }

    QtObject
    {
        id: d

        property int userDefinedColumnsCount: kInvalidColumnsCount

        function getCellHeight(cellIndex)
        {
            const cellWidth = (cellIndex < enlargedCellsCount)
                ? enlargedCellWidth
                : normalCellWidth
            return calculateCellHeight(cellWidth)
        }

        function calculateCellWidth(cellHeight)
        {
            return Math.floor(cellHeight / kAspectRatio)
        }

        function calculateDefaultColumnsCount()
        {
            const maxColumnsCount = Math.max(
                1,
                Math.min(
                    cellsCount,
                    Math.floor((availableWidth + spacing) / (kMinCellWidth + spacing))))

            const cellWidthForMaxColumnsCount =
                Math.floor((availableWidth + spacing) / maxColumnsCount - spacing)
            const cellHeightForMaxColumnsCount = calculateCellHeight(cellWidthForMaxColumnsCount)

            let maxFoundCellHeight = cellHeightForMaxColumnsCount
            let columnsCountForMaxFoundCellHeight = maxColumnsCount

            let columnsCountToTest = maxColumnsCount
            while (columnsCountToTest > 0)
            {
                let rowsCountToTest = Math.ceil(cellsCount / columnsCountToTest)

                if (rowsCountToTest * (kMinCellHeight + spacing) <= (availableHeight + spacing))
                {
                    let cellMaxWidthForTestColumnsCount =
                        Math.floor((availableWidth + spacing) / columnsCountToTest - spacing)

                    let cellMaxHeightForTestRowsCount =
                        Math.ceil((availableHeight + spacing) / rowsCountToTest - spacing)

                    cellMaxHeightForTestRowsCount = Math.min(
                        cellMaxHeightForTestRowsCount,
                        calculateCellHeight(cellMaxWidthForTestColumnsCount))

                    if (cellMaxHeightForTestRowsCount > maxFoundCellHeight)
                    {
                        maxFoundCellHeight = cellMaxHeightForTestRowsCount
                        columnsCountForMaxFoundCellHeight = columnsCountToTest
                    }
                }

                columnsCountToTest -= 1
            }

            return columnsCountForMaxFoundCellHeight
        }

        function calculateNormalCellWidth()
        {
            const cellWidthCorrespondingToAvailableWidth =
                Math.floor((availableWidth + spacing) / columnsCount - spacing)

            if (cellWidthCorrespondingToAvailableWidth < kMinCellWidth)
                return cellWidthCorrespondingToAvailableWidth

            // If a user modified columns count, we do not care about scroll.
            if (d.userDefinedColumnsCount !== defaultColumnsCount
                && d.userDefinedColumnsCount !== kInvalidColumnsCount)
            {
                return cellWidthCorrespondingToAvailableWidth
            }

            const cellHeightCorrespondingToAvailableWidth =
                calculateCellHeight(cellWidthCorrespondingToAvailableWidth)

            const rowsCount = Math.ceil(cellsCount / columnsCount)

            if ((cellHeightCorrespondingToAvailableWidth + spacing) * rowsCount
                <= availableHeight + spacing)
            {
                return cellWidthCorrespondingToAvailableWidth
            }

            const cellHeightCorrespondingToAvailableHeight =
                Math.ceil((availableHeight + spacing) / rowsCount - spacing)
            const cellWidthCorrespondingToAvailableHeight =
                d.calculateCellWidth(cellHeightCorrespondingToAvailableHeight)

            if (cellWidthCorrespondingToAvailableHeight < kMinCellWidth)
                return cellWidthCorrespondingToAvailableWidth

            return cellWidthCorrespondingToAvailableHeight
        }

        function hasFreeVerticalSpace()
        {
            const rowsCount = Math.ceil(cellsCount / columnsCount)
            const normalCellHeight = calculateCellHeight(normalCellWidth)
            return rowsCount * (normalCellHeight + spacing) < availableHeight + spacing
        }

        function calculateEnlargedCellsCount()
        {
            if (!hasFreeVerticalSpace())
                return 0

            return getCellsCountToReorder()
        }

        function calculateEnlargedCellWidth()
        {
            if (enlargedCellsCount === 0)
                return normalCellWidth

            const normalCellHeight = calculateCellHeight(normalCellWidth)

            if (enlargedCellsCount < columnsCount) //< A single row is enlarged.
            {
                const normalRowsCount = Math.ceil(cellsCount / columnsCount) - 1
                const availableHeightForTheEnlargedRow =
                    (availableHeight + spacing) - normalRowsCount * (normalCellHeight + spacing)

                const cellWidthCorrespondingToAvailableWidth =
                    Math.floor((availableWidth + spacing) / enlargedCellsCount - spacing)
                const cellHeightCorrespondingToAvailableWidth =
                    calculateCellHeight(cellWidthCorrespondingToAvailableWidth)

                if (cellHeightCorrespondingToAvailableWidth <= availableHeightForTheEnlargedRow)
                    return cellWidthCorrespondingToAvailableWidth

                const cellHeightCorrespondingToAvailableHeight = availableHeightForTheEnlargedRow
                const cellWidthCorrespondingToAvailableHeight =
                    d.calculateCellWidth(cellHeightCorrespondingToAvailableHeight)

                return cellWidthCorrespondingToAvailableHeight
            }

            // Two rows are enlarged.
            const normalRowsCount = Math.ceil(cellsCount / columnsCount) - 2
            const availableHeightForTwoEnlargedRows =
                (availableHeight + spacing) - normalRowsCount * (normalCellHeight + spacing)

            const enlargedRowWithMoreCellsLength = Math.ceil(enlargedCellsCount / 2)

            const cellWidthCorrespondingToAvailableWidth =
                Math.floor((availableWidth + spacing) / enlargedRowWithMoreCellsLength - spacing)
            const cellHeightCorrespondingToAvailableWidth =
                calculateCellHeight(cellWidthCorrespondingToAvailableWidth)

            if ((cellHeightCorrespondingToAvailableWidth * 2 + spacing)
                <= availableHeightForTwoEnlargedRows)
            {
                return cellWidthCorrespondingToAvailableWidth
            }

            const cellHeightCorrespondingToAvailableHeight =
                Math.floor((availableHeightForTwoEnlargedRows - spacing) / 2)
            const cellWidthCorrespondingToAvailableHeight =
                d.calculateCellWidth(cellHeightCorrespondingToAvailableHeight)

            return cellWidthCorrespondingToAvailableHeight
        }
    }
}

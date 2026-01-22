// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    required property var sizesCalculator

    readonly property int repositionedTailCellsCount:
    {
        if (sizesCalculator.enlargedCellsCount > 0
            || sizesCalculator.columnsCount === 1
            || sizesCalculator.cellsCount % sizesCalculator.columnsCount === 0)
        {
            return 0
        }

        return sizesCalculator.getCellsCountToReorder()
    }

    readonly property int repositionedTailRowsCount:
    {
        if (repositionedTailCellsCount === 0)
            return 0
        return repositionedTailCellsCount < sizesCalculator.columnsCount ? 1 : 2
    }

    readonly property int repositionedCellsInFirstRow: repositionedTailRowsCount > 0
        ? Math.ceil(repositionedTailCellsCount / repositionedTailRowsCount)
        : 0

    // Methods:

    function calculateRowPaddingForCell(cellIndex)
    {
        if (sizesCalculator.columnsCount === 1)
            return d.normalCellsPadding

        if (cellIndex < sizesCalculator.enlargedCellsCount)
        {
            return cellIndex < sizesCalculator.enlargedCellsCountInFirstRow
                ? d.firstEnlargedRowPadding
                : d.secondEnlargedRowPadding
        }

        if (cellIndex >= sizesCalculator.cellsCount - repositionedTailCellsCount)
        {
            const repositionedCellIndex = toRepositionedCellIndex(cellIndex)

            return repositionedCellIndex < repositionedCellsInFirstRow
                ? d.firstRepositionedRowPadding
                : d.secondRepositionedRowPadding
        }

        return d.normalCellsPadding
    }

    // Returns negative value if the cell is not repositioned.
    function toRepositionedCellIndex(cellIndex)
    {
        return cellIndex - (sizesCalculator.cellsCount - repositionedTailCellsCount)
    }

    QtObject
    {
        id: d

        function calculatePadding(cellWidth, rowLength)
        {
            return Math.floor((sizesCalculator.availableWidth + sizesCalculator.spacing
            - (cellWidth + sizesCalculator.spacing) * rowLength) / 2)
        }

        // Normal rows:

        readonly property int normalCellsPadding:
            d.calculatePadding(sizesCalculator.normalCellWidth, sizesCalculator.columnsCount)

        // Enlarged top rows:

        readonly property int firstEnlargedRowPadding:
        {
            if (sizesCalculator.enlargedRowsCount === 0)
                return 0 //< To make potential visual errors noticeable.

            return d.calculatePadding(
                sizesCalculator.enlargedCellWidth,
                sizesCalculator.enlargedCellsCountInFirstRow)
        }

        readonly property int secondEnlargedRowPadding:
        {
            if (sizesCalculator.enlargedCellsCount === 0
                || sizesCalculator.enlargedRowsCount === 1)
            {
                return 0 //< To make potential visual errors noticeable.
            }

            return d.calculatePadding(
                sizesCalculator.enlargedCellWidth,
                sizesCalculator.enlargedCellsCount - sizesCalculator.enlargedCellsCountInFirstRow)
        }

        // Repositioned tail rows:

        readonly property int firstRepositionedRowPadding:
        {
            if (repositionedTailCellsCount === 0)
                return 0 //< To make potential visual errors noticeable.

            return d.calculatePadding(sizesCalculator.normalCellWidth, repositionedCellsInFirstRow)
        }

        readonly property int secondRepositionedRowPadding:
        {
            if (repositionedTailCellsCount === 0 || repositionedTailRowsCount === 1)
                return 0 //< To make potential visual errors noticeable.

            return d.calculatePadding(
                sizesCalculator.normalCellWidth,
                repositionedTailCellsCount - repositionedCellsInFirstRow)
        }
    }
}

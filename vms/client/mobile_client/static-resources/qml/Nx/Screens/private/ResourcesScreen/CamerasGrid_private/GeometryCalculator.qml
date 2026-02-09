// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    id: geometryCalculator

    required property var sizesCalculator

    function calculateCellGeometry(index)
    {
        const x = d.calculateCellX(index)
        const y = d.calculateCellY(index)

        const width = index < sizesCalculator.enlargedCellsCount
            ? sizesCalculator.enlargedCellWidth
            : sizesCalculator.normalCellWidth

        const height = sizesCalculator.cellHeightFromWidth(width)

        return Qt.rect(x, y, width, height)
    }

    PaddingsCalculator
    {
        id: paddingsCalculator

        sizesCalculator: geometryCalculator.sizesCalculator
    }

    QtObject
    {
        id: d

        function calculateCellX(cellIndex)
        {
            const rowPadding = paddingsCalculator.calculateRowPaddingForCell(cellIndex)
            const cellIndexInRow = d.calculateCellIndexInRow(cellIndex)
            const cellWidth = cellIndex < sizesCalculator.enlargedCellsCount
                ? sizesCalculator.enlargedCellWidth
                : sizesCalculator.normalCellWidth
            return rowPadding + cellIndexInRow * (cellWidth + sizesCalculator.spacing)
        }

        function calculateCellY(cellIndex)
        {
            const enlargedRowHeight =
                sizesCalculator.cellHeightFromWidth(sizesCalculator.enlargedCellWidth)

            if (cellIndex < sizesCalculator.enlargedCellsCount)
            {
                if (sizesCalculator.enlargedRowsCount === 1)
                    return 0

                return cellIndex < sizesCalculator.enlargedCellsCountInFirstRow
                    ? 0
                    : enlargedRowHeight + sizesCalculator.spacing
            }

            const normalCellHeight =
                sizesCalculator.cellHeightFromWidth(sizesCalculator.normalCellWidth)

            // Y position of the first normal row (right after enlarged rows).
            const firstNormalRowY = sizesCalculator.enlargedRowsCount > 0
                ? sizesCalculator.enlargedRowsCount * (enlargedRowHeight + sizesCalculator.spacing)
                : 0

            const repositionedTailCellIndex = paddingsCalculator.toRepositionedCellIndex(cellIndex)

            if (repositionedTailCellIndex < 0)
            {
                // Normal (non-repositioned) cell.
                const normalRowIndex = Math.floor((cellIndex - sizesCalculator.enlargedCellsCount)
                    / sizesCalculator.columnsCount)

                return firstNormalRowY + normalRowIndex * (normalCellHeight + sizesCalculator.spacing)
            }

            // Repositioned tail cell.
            const repositionedRowIndex = Math.floor(repositionedTailCellIndex
                / paddingsCalculator.repositionedCellsInFirstRow)

            const normalRowsCount =
                Math.floor((sizesCalculator.cellsCount
                    - paddingsCalculator.repositionedTailCellsCount)
                / sizesCalculator.columnsCount)

            // Y position of the first repositioned row (right after normal rows).
            const firstRepositionedRowY = firstNormalRowY
                + normalRowsCount * (normalCellHeight + sizesCalculator.spacing)

            return firstRepositionedRowY + repositionedRowIndex * (normalCellHeight + sizesCalculator.spacing)
        }

        function calculateCellIndexInRow(cellIndex)
        {
            if (cellIndex < sizesCalculator.enlargedCellsCount)
            {
                return cellIndex < sizesCalculator.enlargedCellsCountInFirstRow
                    ? cellIndex
                    : cellIndex - sizesCalculator.enlargedCellsCountInFirstRow
            }
            else if (cellIndex >=
                sizesCalculator.cellsCount - paddingsCalculator.repositionedTailCellsCount)
            {
                const repositionedCellIndex = paddingsCalculator.toRepositionedCellIndex(cellIndex)
                return repositionedCellIndex % paddingsCalculator.repositionedCellsInFirstRow
            }
            else
            {
                const normalCellIndex = cellIndex - sizesCalculator.enlargedCellsCount
                return normalCellIndex % sizesCalculator.columnsCount
            }
        }
    }
}

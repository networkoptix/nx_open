// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    // Input properties:

    property real availableWidth: 0.0
    property int cellsCount: 0
    property int columnsCount: 0
    property real normalCellWidth: 0.0
    property int enlargedCellsCount: 0
    property real enlargedCellWidth: 0.0
    property real spacing: 0.0

    // Constants:

    readonly property int kPaddingOfCellsInTheMiddleOfRow: 0

    // Normal rows:

    readonly property int normalCellsPadding: d.calculatePadding(normalCellWidth, columnsCount)

    // Enlarged top rows:

    readonly property bool singleRowIsEnlarged: enlargedCellsCount < columnsCount

    readonly property int firstEnlargedRowPadding:
    {
        if (enlargedCellsCount === 0)
            return 0 //< To make potential visual errors noticible.

        const firstRowLength = singleRowIsEnlarged
            ? enlargedCellsCount
            : Math.floor(enlargedCellsCount / 2)

        return d.calculatePadding(enlargedCellWidth, firstRowLength)
    }

    readonly property int secondEnlargedRowPadding:
    {
        if (enlargedCellsCount === 0 || singleRowIsEnlarged)
            return 0 //< To make potential visual errors noticible.

        const secondRowLength = Math.ceil(enlargedCellsCount / 2)

        return d.calculatePadding(enlargedCellWidth, secondRowLength)
    }

    // Repositioned tail rows:

    readonly property int repositionedTailCellsCount:
    {
        if (enlargedCellsCount > 0
            || columnsCount === 1
            || cellsCount % columnsCount === 0)
        {
            return 0
        }

        return sizesCalculator.getCellsCountToReorder()
    }

    readonly property bool singleRowIsRepositioned:
        paddingsCalculator.repositionedTailCellsCount < columnsCount

    readonly property int firstRepositionedRowPadding:
    {
        if (repositionedTailCellsCount === 0)
            return 0 //< To make potential visual errors noticible.

        const firstRowLength = singleRowIsRepositioned
            ? repositionedTailCellsCount
            : Math.ceil(repositionedTailCellsCount / 2)

        return d.calculatePadding(normalCellWidth, firstRowLength)
    }

    readonly property int secondRepositionedRowPadding:
    {
        if (repositionedTailCellsCount === 0 || singleRowIsRepositioned)
            return 0 //< To make potential visual errors noticible.

        const secondRowLength = Math.floor(repositionedTailCellsCount / 2)

        return d.calculatePadding(normalCellWidth, secondRowLength)
    }

    QtObject
    {
        id: d

        function calculatePadding(cellWidth, rowLength)
        {
            return Math.floor((availableWidth + spacing - (cellWidth + spacing) * rowLength) / 2)
        }
    }
}

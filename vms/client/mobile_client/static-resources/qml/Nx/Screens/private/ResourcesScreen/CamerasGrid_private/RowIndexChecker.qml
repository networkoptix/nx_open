// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    id: rowIndexChecker

    property int cellsCount: 0
    property int enlargedCellsCount: 0

    // Enlarged top rows:

    function isEnlargedCell(index)
    {
        return index < enlargedCellsCount
    }

    function isFirstEnlargedRowBeginning(index)
    {
        if (enlargedCellsCount === 0)
            return false

        const firstEnlargedRowBeginningIndex = 0
        return index === firstEnlargedRowBeginningIndex
    }

    function isSecondEnlargedRowBeginning(index)
    {
        if (enlargedCellsCount === 0 || paddingsCalculator.singleRowIsEnlarged)
            return false

        const secondEnlargedRowBeginningIndex = Math.floor(enlargedCellsCount / 2)
        return index === secondEnlargedRowBeginningIndex
    }

    // Repositioned tail rows:

    function isRepositionedTailCell(index)
    {
        return index >= cellsCount - paddingsCalculator.repositionedTailCellsCount
    }

    function isFirstRepositionedTailRowBeginning(index)
    {
        const firstRepositionedTailRowBeginningIndex =
            cellsCount - paddingsCalculator.repositionedTailCellsCount
        return index === firstRepositionedTailRowBeginningIndex
    }

    function isSecondRepositionedTailRowBeginning(index)
    {
        if (paddingsCalculator.singleRowIsRepositioned)
            return false

        const secondRepositionedRowLength =
            Math.floor(paddingsCalculator.repositionedTailCellsCount / 2)

        const secondRepositionedTailRowBeginningIndex = cellsCount - secondRepositionedRowLength
        return index === secondRepositionedTailRowBeginningIndex
    }
}

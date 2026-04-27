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
    readonly property int minColumnsCount: d.calculateMinColumnsCount()
    readonly property int defaultColumnsCount: d.calculateDefaultColumnsCount()

    readonly property int columnsCount:
        d.userDefinedColumnsCount === kInvalidColumnsCount
            ? defaultColumnsCount
            : MathUtils.bound(minColumnsCount, d.userDefinedColumnsCount, defaultColumnsCount)

    readonly property bool widthFillingZoomAvailable:
    {
        const scrollIsRequired =
            (kMinCellHeight + spacing) * cellsCount > availableHeight + spacing

        const thereIsUnusedHeightWhenCellWidthIsMaximized =
            (availableWidth * kAspectRatio + spacing) * cellsCount <= availableHeight + spacing

        return defaultColumnsCount === 1 && !scrollIsRequired
            && !thereIsUnusedHeightWhenCellWidthIsMaximized
    }

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

    onWidthFillingZoomAvailableChanged:
    {
        if (!widthFillingZoomAvailable)
            setWidthFillingZoomEnabled(false)
    }

    function setUserDefinedColumnsCount(newColumnsCount)
    {
        d.userDefinedColumnsCount = newColumnsCount
    }

    function rawUserDefinedColumnsCount()
    {
        return d.userDefinedColumnsCount
    }

    function widthFillingZoomEnabled()
    {
        return d.widthFillingZoomEnabled
    }

    function setWidthFillingZoomEnabled(enabled)
    {
        if (widthFillingZoomAvailable && d.widthFillingZoomEnabled === enabled)
            return

        d.widthFillingZoomEnabled = widthFillingZoomAvailable ? enabled : false
    }

    readonly property int totalLayoutHeight:
    {
        const enlargedRowsTotalHeight = enlargedRowsCount > 0
            ? enlargedRowsCount * (cellHeightFromWidth(enlargedCellWidth) + spacing) - spacing
            : 0

        const normalRowsCount = Math.ceil((cellsCount - enlargedCellsCount) / columnsCount)
        const normalRowsTotalHeight = normalRowsCount > 0
            ? normalRowsCount * (cellHeightFromWidth(normalCellWidth) + spacing) - spacing
            : 0

        return enlargedRowsTotalHeight + normalRowsTotalHeight
            + ((enlargedRowsCount > 0 && normalRowsCount > 0) ? spacing : 0)
    }

    function cellHeightFromWidth(cellWidth)
    {
        return d.roundDimension(cellWidth * kAspectRatio)
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

        // Raw user-defined value. May be out of [minColumnsCount, defaultColumnsCount] after
        // geometry change or a reload with a different camera count. kInvalidColumnsCount means
        // the user has not customized and columnsCount tracks defaultColumnsCount.
        // The user-defined value is stored raw so that the user's intent survives transient states:
        // the load running before cellsCount is final (first camera arrived but others still coming
        // from the resource pool), and geometry changes that temporarily reduce defaultColumnsCount
        // (e.g. rotation to portrait).
        property int userDefinedColumnsCount: kInvalidColumnsCount

        property bool widthFillingZoomEnabled: false

        // Round off logic must be similar for any calculation to avoid visual artifacts.
        function roundDimension(realWidthOrHeightValue)
        {
            return Math.floor(realWidthOrHeightValue)
        }

        function getCellHeight(cellIndex)
        {
            const cellWidth = (cellIndex < enlargedCellsCount)
                ? enlargedCellWidth
                : normalCellWidth
            return cellHeightFromWidth(cellWidth)
        }

        function cellWidthFromHeight(cellHeight)
        {
            return roundDimension(cellHeight / kAspectRatio)
        }

        function calculateMinColumnsCount()
        {
            const maxCellWidthFromHeight = d.cellWidthFromHeight(availableHeight)

            return MathUtils.bound(
                1,
                Math.ceil((availableWidth + spacing) / (maxCellWidthFromHeight + spacing)),
                defaultColumnsCount)
        }

        function calculateDefaultColumnsCount()
        {
            const maxColumnsCount = Math.max(
                1,
                Math.min(
                    cellsCount,
                    Math.floor((availableWidth + spacing) / (kMinCellWidth + spacing))))

            const cellWidthForMaxColumnsCount =
                roundDimension((availableWidth + spacing) / maxColumnsCount - spacing)
            const cellHeightForMaxColumnsCount = cellHeightFromWidth(cellWidthForMaxColumnsCount)

            let maxFoundCellHeight = cellHeightForMaxColumnsCount
            let columnsCountForMaxFoundCellHeight = maxColumnsCount

            let columnsCountToTest = maxColumnsCount
            while (columnsCountToTest > 0)
            {
                let rowsCountToTest = Math.ceil(cellsCount / columnsCountToTest)

                if (rowsCountToTest * (kMinCellHeight + spacing) <= (availableHeight + spacing))
                {
                    let cellMaxWidthForTestColumnsCount =
                        roundDimension((availableWidth + spacing) / columnsCountToTest - spacing)

                    let cellMaxHeightForTestRowsCount =
                        roundDimension((availableHeight + spacing) / rowsCountToTest - spacing)

                    cellMaxHeightForTestRowsCount = Math.min(
                        cellMaxHeightForTestRowsCount,
                        cellHeightFromWidth(cellMaxWidthForTestColumnsCount))

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
                roundDimension((availableWidth + spacing) / columnsCount - spacing)

            if (cellWidthCorrespondingToAvailableWidth < kMinCellWidth)
                return cellWidthCorrespondingToAvailableWidth

            if (widthFillingZoomAvailable && d.widthFillingZoomEnabled)
                return cellWidthCorrespondingToAvailableWidth

            // If a user explicitly chose a columns count different from default that's currently
            // achievable, honor it (width-fill, ignore scroll). Compare against the clamped
            // columnsCount, not the raw userDefinedColumnsCount: a stored userDef that's out of
            // the current [minColumnsCount, defaultColumnsCount] range would otherwise trigger
            // this branch and stretch cells to full width even though the user's intent
            // is unreachable - columnsCount was already clamped to defaultColumnsCount.
            if (columnsCount !== defaultColumnsCount)
                return cellWidthCorrespondingToAvailableWidth

            const cellHeightCorrespondingToAvailableWidth =
                cellHeightFromWidth(cellWidthCorrespondingToAvailableWidth)

            const rowsCount = Math.ceil(cellsCount / columnsCount)

            if ((cellHeightCorrespondingToAvailableWidth + spacing) * rowsCount
                <= availableHeight + spacing)
            {
                return cellWidthCorrespondingToAvailableWidth
            }

            const cellHeightCorrespondingToAvailableHeight =
                roundDimension((availableHeight + spacing) / rowsCount - spacing)
            const cellWidthCorrespondingToAvailableHeight =
                d.cellWidthFromHeight(cellHeightCorrespondingToAvailableHeight)

            if (cellWidthCorrespondingToAvailableHeight < kMinCellWidth)
                return cellWidthCorrespondingToAvailableWidth

            return cellWidthCorrespondingToAvailableHeight
        }

        function scrollIsRequired()
        {
            const rowsCount = Math.ceil(cellsCount / columnsCount)
            const normalCellHeight = cellHeightFromWidth(normalCellWidth)
            return rowsCount * (normalCellHeight + spacing) > availableHeight + spacing
        }

        function calculateEnlargedCellsCount()
        {
            if (scrollIsRequired())
                return 0

            return getCellsCountToReorder()
        }

        function calculateEnlargedCellWidth()
        {
            if (enlargedCellsCount === 0)
                return normalCellWidth

            const normalCellHeight = cellHeightFromWidth(normalCellWidth)
            const spacingBetweenNormalAndEnlargedRows = spacing

            if (enlargedCellsCount < columnsCount) //< A single row is enlarged.
            {
                const normalRowsCount = Math.ceil(cellsCount / columnsCount) - 1
                const availableHeightForTheEnlargedRow =
                    (availableHeight + spacing) - normalRowsCount * (normalCellHeight + spacing)
                    - spacingBetweenNormalAndEnlargedRows

                const cellWidthCorrespondingToAvailableWidth =
                    roundDimension((availableWidth + spacing) / enlargedCellsCount - spacing)
                const cellHeightCorrespondingToAvailableWidth =
                    cellHeightFromWidth(cellWidthCorrespondingToAvailableWidth)

                if (cellHeightCorrespondingToAvailableWidth <= availableHeightForTheEnlargedRow)
                    return cellWidthCorrespondingToAvailableWidth

                const cellHeightCorrespondingToAvailableHeight = availableHeightForTheEnlargedRow
                const cellWidthCorrespondingToAvailableHeight =
                    d.cellWidthFromHeight(cellHeightCorrespondingToAvailableHeight)

                return cellWidthCorrespondingToAvailableHeight
            }

            // Two rows are enlarged.
            const normalRowsCount = Math.ceil(cellsCount / columnsCount) - 2
            const availableHeightForTwoEnlargedRows =
                (availableHeight + spacing) - normalRowsCount * (normalCellHeight + spacing)
                - spacingBetweenNormalAndEnlargedRows

            const enlargedRowWithMoreCellsLength = Math.ceil(enlargedCellsCount / 2)

            const cellWidthCorrespondingToAvailableWidth =
                roundDimension((availableWidth + spacing) / enlargedRowWithMoreCellsLength
                    - spacing)
            const cellHeightCorrespondingToAvailableWidth =
                cellHeightFromWidth(cellWidthCorrespondingToAvailableWidth)

            if ((cellHeightCorrespondingToAvailableWidth * 2 + spacing)
                <= availableHeightForTwoEnlargedRows)
            {
                return cellWidthCorrespondingToAvailableWidth
            }

            const cellHeightCorrespondingToAvailableHeight =
                roundDimension((availableHeightForTwoEnlargedRows - spacing) / 2)
            const cellWidthCorrespondingToAvailableHeight =
                d.cellWidthFromHeight(cellHeightCorrespondingToAvailableHeight)

            return cellWidthCorrespondingToAvailableHeight
        }
    }
}

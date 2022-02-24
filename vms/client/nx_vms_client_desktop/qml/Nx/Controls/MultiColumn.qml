// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

Grid
{
    property int maxColumns: 4
    property int minItemsPerColumn: 2

    property alias delegate: repeater.delegate
    property alias model: repeater.model
    property alias count: repeater.count

    rowSpacing: 8
    columnSpacing: 16

    columns: count / minItemsPerColumn

    flow: Grid.TopToBottom

    Repeater
    {
        id: repeater
    }

    function updateColumns()
    {
        columns = Math.min(count / minItemsPerColumn, maxColumns)

        while (implicitWidth > width && columns > 1)
            --columns
    }

    function itemAt(index)
    {
        return repeater.itemAt(index)
    }

    onWidthChanged:
        updateColumns()

    onMaxColumnsChanged:
        updateColumns()

    onMinItemsPerColumnChanged:
        updateColumns()
}

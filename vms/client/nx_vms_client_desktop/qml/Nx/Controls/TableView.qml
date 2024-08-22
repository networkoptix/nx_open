// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

import Nx.Controls
import Nx.Core
import Nx.Models

import nx.vms.client.desktop

import "private"

TableView
{
    id: control

    property var sourceModel: null
    property alias horizontalHeaderVisible: columnsHeader.visible
    property alias headerBackgroundColor: columnsHeader.color
    property alias horizontalHeaderEnabled: columnsHeader.enabled

    property alias headerDelegate: repeater.delegate

    property alias deleteShortcut: deleteShortcut
    property alias enterShortcut: enterShortcut

    property int hoveredRow: hoverHandler.hovered
        ? control.rowAtPosition(hoverHandler.point.position.x, hoverHandler.point.position.y, /*includeSpacing*/ true)
        : -1

    // Whether column width must be equal to the appropriate header delegate width.
    property var useDelegateWidthAsColumnWidth: (column => false)

    function rowAtPosition(x, y, includeSpacing)
    {
        const cellUnderMouse = cellAtPosition(x, y, includeSpacing)
        const indexUnderCursor = modelIndex(cellUnderMouse)
        return rowAtIndex(indexUnderCursor)
    }

    function getButtonItem(columnIndex)
    {
        return repeater.itemAt(columnIndex)
    }

    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    topMargin: columnsHeader.visible ? columnsHeader.height : 0

    onWidthChanged:
    {
        impl.updateColumnWidths()
    }

    onColumnsChanged:
    {
        impl.updateColumnWidths()
    }

    QtObject
    {
        id: impl

        readonly property int minColumnWidth: 28
        property var columnWidthCache: []

        function updateColumnWidths()
        {
            if (columns < 0)
                return

            columnWidthCache = new Array(columns)

            if (!control.model)
                return

            let assignedColumnCount = 0
            let assignedSpace = 0
            // First, assign width for the columns with the fixed width.
            for (let column = 0; column < columns; ++column)
            {
                if (useDelegateWidthAsColumnWidth(column))
                {
                    columnWidthCache[column] =
                        Math.max(getButtonItem(column).implicitWidth, minColumnWidth)
                    assignedSpace += columnWidthCache[column]
                    ++assignedColumnCount
                    continue
                }

                const sizeHintValue = control.model.headerData(column, Qt.Horizontal, Qt.SizeHintRole)
                if (sizeHintValue != null)
                {
                    columnWidthCache[column] = Math.max(sizeHintValue.width, minColumnWidth)
                    assignedSpace += columnWidthCache[column]
                    ++assignedColumnCount
                    continue
                }

                columnWidthCache[column] = 0
            }

            // Assign all the rest space between the rest columns equally.
            const defaultColumnWidth = Math.max(
                (control.width - assignedSpace) / ((control.columns - assignedColumnCount) || 1),
                minColumnWidth)
            for (let i = 0; i < columns; ++i)
            {
                if (columnWidthCache[i] === 0)
                    columnWidthCache[i] = defaultColumnWidth
            }

            // By some reasons delegates might not be resized, call forceLayout fix the problem.
            Qt.callLater(control.forceLayout)
        }
    }

    columnWidthProvider: (column) => impl.columnWidthCache[column]

    ScrollBar.vertical: ScrollBar
    {
        parent: control.parent //< The only way to shift ScrollBar yCoord and change height.

        x: control.x + control.width - width
        y: control.y + (columnsHeader.visible ? columnsHeader.height : 0)
        height: control.height - (columnsHeader.visible ? columnsHeader.height : 0)
    }

    delegate: BasicSelectableTableCellDelegate {}

    onLayoutChanged:
    {
        for (let columnIndex = 0; columnIndex < repeater.count; ++columnIndex)
        {
            let headerItem = repeater.itemAt(columnIndex)
            headerItem.width = control.columnWidthProvider(columnIndex)
        }
    }

    model: SortFilterProxyModel
    {
        id: sortFilterProxyModel
        sourceModel: control.sourceModel
    }

    selectionModel: ItemSelectionModel{}

    Rectangle
    {
        id: columnsHeader

        readonly property int buttonHeight: 32

        y: control.contentY
        z: 2 //< Is needed for hiding scrolled rows.
        width: parent.width
        height: buttonHeight + separator.height + 8

        color: ColorTheme.colors.dark7

        visible: false

        Row
        {
            Repeater
            {
                id: repeater

                model: control.columns > 0 ? control.columns : 1

                delegate: HeaderButton
                {
                    id: headerButton
                    model: control.model
                    width: control.columnWidthProvider(index)
                    height: columnsHeader.buttonHeight
                    visible: width > 0
                }
            }
        }

        Rectangle
        {
            id: separator

            y: columnsHeader.buttonHeight

            width: control.width
            height: 1

            color: ColorTheme.colors.dark12
        }
    }

    HoverHandler
    {
        id: hoverHandler
    }

    Shortcut
    {
        id: deleteShortcut
        sequences: Qt.platform.os === "osx" ? ["Backspace", "Delete"] : ["Delete"]
        enabled: control.activeFocus
    }

    Shortcut
    {
        id: enterShortcut
        sequences: ["Enter", "Return"]
        enabled: control.activeFocus
    }

    Keys.onPressed: (event) =>
    {
        // At the moment only row selection is supported.
        // TODO: #mmalofeev add row selection for the ListNavigationHelper and use it here and in the delegate.
        if (selectionBehavior !== TableView.SelectRows)
            return

        if (!selectionModel.hasSelection)
            return

        if (event.key == Qt.Key_Down || event.key == Qt.Key_Up
            || event.key == Qt.Key_Left || event.key == Qt.Key_Right)
        {
            let offset = event.key == Qt.Key_Down || event.key == Qt.Key_Right ? 1 : -1
            const rowToSelect = MathUtils.bound(
                0,
                control.selectionModel.selectedIndexes[0].row + offset,
                control.rows - 1)
            selectionModel.select(
                control.index(rowToSelect, 0),
                ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)

            event.accepted = true;
        }

        // TODO: #mmalofeev add Key_Escape handler.
    }
}

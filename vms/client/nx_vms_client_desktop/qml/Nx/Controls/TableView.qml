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

    property bool editing: false

    property bool horizontalHeaderVisible: false
    property alias headerBackgroundColor: columnsHeader.color
    property bool horizontalHeaderEnabled: true
    property alias headerDelegate: headerRow.delegate

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

    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    topMargin: impl.reservedHeight

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

        readonly property real reservedHeight:
            control.horizontalHeaderVisible ? columnsHeader.height : 0

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
                    columnWidthCache[column] = Math.max(headerRow.columnWidth(column), minColumnWidth)
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
        y: control.y + impl.reservedHeight
        height: control.height - impl.reservedHeight
    }

    ScrollBar.horizontal: ScrollBar
    {
        policy: ScrollBar.AsNeeded
    }

    delegate: BasicSelectableTableCellDelegate {}


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
        x: control.contentX
        y: control.contentY
        z: 2 //< Is needed for hiding scrolled rows.
        // Have to use the width of the header since the width of the control can be smaller.
        width: control.contentItem.width
        height: buttonHeight + separator.height + 8

        color: ColorTheme.colors.dark7

        visible: control.horizontalHeaderVisible
        enabled: control.horizontalHeaderEnabled && !control.editing

        HorizontalHeaderView
        {
            id: headerRow

            syncView: control
            delegate: HeaderButton
            {
                width: control.columnWidthProvider(index)
                height: columnsHeader.buttonHeight
                visible: width > 0
                text: model.display
                sortOrder: control.model.sortColumn === index ? control.model.sortOrder : undefined
                onClicked: control.model.sort(index, nextSortOrder())
            }
        }

        Rectangle
        {
            id: separator

            y: columnsHeader.buttonHeight

            width: parent.width
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
        enabled: control.activeFocus && !control.editing
    }

    Shortcut
    {
        id: enterShortcut
        sequences: ["Enter", "Return"]
        enabled: control.activeFocus && !control.editing
    }

    Keys.onPressed: (event) =>
    {
        if (editing)
            return

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

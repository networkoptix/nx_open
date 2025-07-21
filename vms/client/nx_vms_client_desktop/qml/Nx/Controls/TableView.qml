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
    property alias horizontalHeaderView: headerView
    property bool sortEnabled: true

    property alias deleteShortcut: deleteShortcut
    property alias enterShortcut: enterShortcut

    readonly property int hoveredRow:
    {
        if (!hoverHandler.hovered)
            return -1

        return control.rowAtPosition(hoverHandler.point.position.x, hoverHandler.point.position.y,
            /*includeSpacing*/ true)
    }

    readonly property Item hoveredItem:
    {
        if (!hoverHandler.hovered)
            return null

        return control.itemAtCell(control.cellAtPosition(
            hoverHandler.point.position.x, hoverHandler.point.position.y, /*includeSpacing*/ true))
    }

    // Whether column width must be equal to the appropriate header delegate width.
    property var useDelegateWidthAsColumnWidth: (column => false)

    function rowAtPosition(x, y, includeSpacing)
    {
        const cellAtPos = cellAtPosition(x, y, includeSpacing)
        const indexAtPos = modelIndex(cellAtPos)
        return rowAtIndex(indexAtPos)
    }

    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    topMargin: control.horizontalHeaderVisible ? columnsHeader.height : 0

    columnWidthProvider: function(column)
    {
        if (!isColumnLoaded(column))
            return -1

        return width / columns
    }

    ScrollBar.vertical: ScrollBar
    {
        parent: control.parent //< The only way to shift ScrollBar yCoord and change height.

        x: control.x + control.width - width
        y: control.y + control.topMargin
        height: control.height - control.topMargin
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
            id: headerView

            objectName: "headerView"
            syncView: control

            delegate: Rectangle
            {
                id: headerDelegate

                implicitWidth: control.columnWidthProvider(index)
                implicitHeight: columnsHeader.buttonHeight
                color: headerBackgroundColor
                visible: width > 0

                HeaderButton
                {
                    objectName: "headerButton"
                    anchors.fill: parent
                    text: model.display

                    sortOrder: (control?.sortEnabled ?? false) && control.model.sortColumn === index
                        ? control.model.sortOrder
                        : undefined

                    onClicked:
                    {
                        if (control.sortEnabled)
                            control.model.sort(index, nextSortOrder())
                    }
                }
            }
            boundsBehavior: control.boundsBehavior
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

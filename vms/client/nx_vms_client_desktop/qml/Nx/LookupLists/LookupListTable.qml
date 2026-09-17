// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import Qt.labs.qmlmodels

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.core.analytics as Analytics

CheckableTableView
{
    id: control

    required property Analytics.StateView taxonomy
    signal valueChanged

    columnSpacing: 0
    rowSpacing: 0

    columnWidthProvider: function (column)
    {
        if (!column)
            return 35

        if (control.model && control.model.rowCount() === 0)
        {
            // With 0 rows no column width is loaded yet, so use the header text width instead.
            return Math.max(100, headerTextWidth(column))
        }

        return Math.max(100, columnWidth(column))
    }

    horizontalHeaderVisible: true
    horizontalHeaderView.resizableColumns: false
    horizontalHeaderEnabled: !editing

    function headerTextWidth(column)
    {
        const text = control.model.headerData(column, Qt.Horizontal) ?? ""
        return headerFontMetrics.advanceWidth(text)
    }

    function rowContainsCorrectData(row)
    {
        for (let i = 0; i < control.model.columnCount(); ++i)
        {
            if (control.model.data(control.model.index(row, i), LookupListEntriesModel.RawValueRole))
                return true
        }
        return false
    }

    function sortTable()
    {
        control.model.sort(1)
    }

    FontMetrics
    {
        id: headerFontMetrics
        font.pixelSize: 14
        font.weight: Font.Medium
    }

    Binding
    {
        target: control
        property: "contentWidth"
        when: control.model && control.model.rowCount() === 0

        // With 0 rows, contentWidth is never computed automatically, so calculate it here.
        value:
        {
            let totalWidth = 0
            for (let column = 0; column < control.columns; ++column)
                totalWidth += control.columnWidthProvider(column)
            return totalWidth
        }
    }

    delegate: DelegateChooser
    {
        DelegateChoice
        {
            column: 0

            BasicSelectableTableCheckableCellDelegate
            {
                enabled: !editing
            }
        }

        DelegateChoice
        {
            Rectangle
            {
                required property bool selected
                property bool hovered: TableView.view ? TableView.view.hoveredRow === row : false

                implicitHeight: 28
                color: selected ? ColorTheme.colors.dark9 : (hovered ? ColorTheme.colors.dark8 : ColorTheme.colors.dark7)

                LookupListTableCellDelegate
                {
                    id: delegateItem

                    onEditingStarted: editing = true
                    onEditingFinished: editing = false
                    onValueChanged: (newValue, previousValue) =>
                    {
                        if (!control.rowContainsCorrectData(row))
                            revert(previousValue) //<= Full row contains only empty values, reverting.
                        else
                            control.valueChanged()
                    }
                    Component.onDestruction: editing = false

                    taxonomy: control.taxonomy
                }
            }
        }
    }
}

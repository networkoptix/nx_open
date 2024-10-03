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
    property bool editing: false //< Shouldn't be changed outside this file.
    signal valueChanged

    columnSpacing: 0
    rowSpacing: 0

    ScrollBar.horizontal: ScrollBar
    {
        policy: ScrollBar.AsNeeded
    }

    columnWidthProvider: function (column)
    {
        return column ? Math.max(100, columnWidth(column)) : 35
    }

    horizontalHeaderVisible: true
    horizontalHeaderEnabled: !editing

    function rowContainsCorrectData(row)
    {
        for (let i = 0; i < control.model.columnCount(); ++i)
        {
            if (control.model.data(control.model.index(row, i)))
                return true
        }
        return false
    }

    function sortTable()
    {
        control.model.sort(1)
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

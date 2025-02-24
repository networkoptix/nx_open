// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Qt.labs.qmlmodels

import Nx.Controls

import nx.vms.client.desktop

import "private"

TableView
{
    id: root

    function removeCheckedRows()
    {
        rowCheckModel.removeCheckedRows()
    }

    readonly property bool hasCheckedRows: checkedRows.length > 0
    property alias checkedRows: rowCheckModel.checkedRows

    useDelegateWidthAsColumnWidth: function (column)
    {
        return column === 0
    }

    model: SortFilterProxyModel
    {
        sourceModel: RowCheckModel
        {
            id: rowCheckModel
            sourceModel: root.sourceModel
        }

        sortRole: root.sourceModel.sortRole ?? Qt.DisplayRole
        sortCaseSensitivity: Qt.CaseInsensitive
    }

    headerDelegate: DelegateChooser
    {
        DelegateChoice
        {
            column: 0
            CheckableHeaderButton
            {
                model: rowCheckModel
                width: root.columnWidthProvider(index)
            }
        }

        DelegateChoice
        {
            HeaderButton
            {
                text: model.display
                width: root.columnWidthProvider(index)
                leftPadding: 8
                rightPadding: 8
                sortOrder: root.model.sortColumn === index ? root.model.sortOrder : undefined
                onClicked: root.model.sort(index, nextSortOrder())
            }
        }
    }

    delegate: DelegateChooser
    {
        DelegateChoice
        {
            column: 0
            BasicSelectableTableCheckableCellDelegate
            {
                enabled: !root.editing
            }
        }

        DelegateChoice
        {
            BasicSelectableTableCellDelegate {}
        }
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Qt.labs.qmlmodels

import Nx.Controls

import nx.vms.client.desktop

import "private"

TableView
{
    id: root

    readonly property bool hasCheckedRows: checkedRows.length > 0
    property alias checkedRows: rowCheckModel.checkedRows

    useDelegateWidthAsColumnWidth: function (column)
    {
        return column === 0
    }

    model: RowCheckModel
    {
        id: rowCheckModel
        sourceModel: root.sourceModel
    }

    headerDelegate: DelegateChooser
    {
        DelegateChoice
        {
            index: 0
            CheckableHeaderButton
            {
                model: root.model
                width: root.columnWidthProvider(index)
            }
        }

        DelegateChoice
        {
            HeaderButton
            {
                model: root.model
                width: root.columnWidthProvider(index)
            }
        }
    }

    delegate: DelegateChooser
    {
        DelegateChoice
        {
            column: 0
            BasicSelectableTableCheckableCellDelegate {}
        }

        DelegateChoice
        {
            BasicSelectableTableCellDelegate {}
        }
    }
}

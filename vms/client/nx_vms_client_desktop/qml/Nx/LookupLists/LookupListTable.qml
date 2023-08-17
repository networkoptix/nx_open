// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import Qt.labs.qmlmodels

import Nx
import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

TableView
{
    id: control

    required property Analytics.StateView taxonomy
    signal editingStarted
    signal editingFinished

    function getColumnWidth(columnIndex)
    {
        let w = explicitColumnWidth(columnIndex)
        if (w >= 0)
            return w

        return -1
    }

    columnSpacing: 0
    rowSpacing: 0

    showCheckboxColumn: true
    horizontalHeaderVisible: true

    columnWidthProvider: function (columnIndex)
    {
        if (isCheckboxColumn(control.model, columnIndex))
            return Math.max(getButtonItem(columnIndex).implicitWidth, 28)

        // Stretch the last column to the entire free width.
        if (columnIndex === columns - 1)
        {
            let occupiedSpace = 0
            for (let i = 0; i < columnIndex; ++i)
            {
                const w1 = getColumnWidth(i)
                console.log("Width of " + i + " is " + w1)
                occupiedSpace += getColumnWidth(i)
            }

            // For some unknown yet reason we need to hardcode an indentation here.
            const lastColumnWidth = width - occupiedSpace - 10
            const lastColumnMinSize = getColumnWidth(columnIndex)
            return Math.max(lastColumnWidth, lastColumnMinSize)
        }

        return getColumnWidth(columnIndex)
    }

    DelegateChooser
    {
        id: chooser

        DelegateChoice
        {
            id: delegateChoice

            column: 0

            Rectangle
            {
                implicitWidth: 28
                implicitHeight: 28
                color: ColorTheme.colors.dark7

                CheckBox
                {
                    id: checkBox

                    x: 8
                    y: 6
                    anchors.verticalCenter: parent.verticalCenter
                    checked: model.display
                    onCheckStateChanged:
                    {
                        if (checkState !== model.display)
                            model.edit = checkState
                    }
                }
            }
        }

        DelegateChoice
        {
            Rectangle
            {
                implicitWidth: Math.max(124, delegateItem.implicitWidth)
                implicitHeight: 28
                color: ColorTheme.colors.dark7
                required property bool selected

                LookupListTableCellDelegate
                {
                    id: delegateItem

                    onEditingStarted: control.editingStarted()
                    onEditingFinished: control.editingFinished()
                    Component.onDestruction: control.editingFinished()

                    taxonomy: control.taxonomy
                }
            }
        }
    }

    delegate: chooser
}

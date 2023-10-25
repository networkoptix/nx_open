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
    property bool editing: false //< Shouldn't be changed outside this file.
    signal valueChanged

    columnSpacing: 0
    rowSpacing: 0

    showCheckboxColumn: true
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
                    enabled: !editing
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

    delegate: chooser
}

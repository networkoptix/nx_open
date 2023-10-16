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
    signal valueChanged

    columnSpacing: 0
    rowSpacing: 0

    showCheckboxColumn: true
    horizontalHeaderVisible: true

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
                    onValueChanged: control.valueChanged()
                    Component.onDestruction: control.editingFinished()

                    taxonomy: control.taxonomy
                }
            }
        }
    }

    delegate: chooser
}

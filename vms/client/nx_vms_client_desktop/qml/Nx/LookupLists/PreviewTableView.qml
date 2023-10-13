// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick as CoreItems

import Nx
import Nx.Core
import Nx.Controls
import Nx.Models

import nx.vms.client.desktop

CoreItems.TableView
{
    id: control

    flickableDirection: CoreItems.Flickable.VerticalFlick
    boundsBehavior: CoreItems.Flickable.StopAtBounds
    topMargin: columnsHeader.height

    // TODO: make width of header and elements dynamic
    columnWidthProvider: function (column)
    {
        return columnsHeader.comboBoxWidth + columnsHeader.comboBoxSpacing
    }

    ScrollBar.vertical: ScrollBar
    {
        parent: control.parent //< The only way to shift ScrollBar yCoord and change height.

        x: control.x + control.width - width
        y: control.y + (columnsHeader.visible ? columnsHeader.height : 0)
        height: control.height - (columnsHeader.visible ? columnsHeader.height : 0)
    }

    delegate: BasicTableCellDelegate
    {
        leftPadding: 8
        bottomPadding: 6
        rightPadding: 8
        color: ColorTheme.colors.light10
    }

    CoreItems.Rectangle
    {
        id: columnsHeader

        readonly property int comboBoxHeight: 28
        readonly property int comboBoxWidth: 128
        readonly property int comboBoxSpacing: 8

        y: control.contentY
        z: 2 //< Is needed for hiding scrolled rows.
        width: parent.width
        height: comboBoxHeight + 8
        color: ColorTheme.colors.dark7

        CoreItems.Row
        {
            spacing: columnsHeader.comboBoxSpacing
            CoreItems.Repeater
            {
                model: control.columns > 0 ? control.columns : 1

                ComboBox
                {
                    id: headerComboBox

                    width: columnsHeader.comboBoxWidth
                    height: columnsHeader.comboBoxHeight
                    visible: control.model.rowCount()
                    model: control.model ? [control.model.headerData(index, Qt.Horizontal)] : []
                }
            }
        }
    }
}

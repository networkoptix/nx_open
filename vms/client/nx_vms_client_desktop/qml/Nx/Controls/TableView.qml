// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Controls
import Nx.Core
import Nx.Models

import nx.vms.client.desktop

import "private"

TableView
{
    id: control

    property bool showCheckboxColumn: false

    property alias horizontalHeaderVisible: columnsHeader.visible
    property alias headerBackgroundColor: columnsHeader.color
    signal headerCheckStateChanged()

    function getSelectedRowIndexes()
    {
        if (!showCheckboxColumn)
            return []

        return rowSelectionModel.getSelectedRows()
    }

    function getButtonItem(columnIndex)
    {
        return repeater.itemAt(columnIndex)
    }

    function isCheckboxColumn(model, columnIndex)
    {
        if (!model)
            return false

        return model.headerData(
            columnIndex,
            Qt.Horizontal,
            Qt.CheckStateRole) != null
    }

    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    topMargin: columnsHeader.visible ? columnsHeader.height : 0

    columnWidthProvider: function (column)
    {
        if (isCheckboxColumn(control.model, column))
            return Math.max(getButtonItem(column).implicitWidth, 28)
        return control.width / (control.columns || 1)
    }

    ScrollBar.vertical: ScrollBar
    {
        parent: control.parent //< The only way to shift ScrollBar yCoord and change height.

        x: control.x + control.width - width
        y: control.y + (columnsHeader.visible ? columnsHeader.height : 0)
        height: control.height - (columnsHeader.visible ? columnsHeader.height : 0)
    }

    delegate: BasicTableCellDelegate {}

    onLayoutChanged:
    {
        for (let columnIndex = 0; columnIndex < repeater.count; ++columnIndex)
        {
            let headerItem = repeater.itemAt(columnIndex)
            headerItem.width = headerItem.calculateWidth(control.columnWidthProvider(columnIndex))
        }
    }

    onModelChanged:
    {
        if (!showCheckboxColumn)
            return

        if (model !== rowSelectionModel.sourceModel
            && model !== rowSelectionModel)
        {
            rowSelectionModel.sourceModel = model
            model = rowSelectionModel
            control.forceLayout()
        }
    }

    RowSelectionModel
    {
        id: rowSelectionModel
    }

    Rectangle
    {
        id: columnsHeader

        readonly property int buttonHeight: 32

        y: control.contentY
        z: 2 //< Is needed for hiding scrolled rows.
        width: parent.width
        height: buttonHeight + separator.height + 8

        color: ColorTheme.colors.dark7

        visible: false

        Row
        {
            Repeater
            {
                id: repeater

                model: control.columns > 0 ? control.columns : 1

                HeaderButton
                {
                    id: headerButton

                    function calculateWidth(desiredWidth)
                    {
                        if (desiredWidth >= 0)
                            return desiredWidth

                        const maxImplicitWidth = Math.max(implicitWidth, implicitColumnWidth(index))

                        return maxImplicitWidth < control.width ? maxImplicitWidth : control.width
                    }

                    property var headerDataAccessor: ModelDataAccessor
                    {
                        function updateCheckState()
                        {
                            if (!isCheckbox)
                                return

                            let hasCheckedValues = false
                            let hasUncheckedValues = false

                            for (let row = 0; row < control.model.rowCount(); ++row)
                            {
                                const val = headerDataAccessor.getData(
                                    control.model.index(row, index),
                                    "display")

                                if (val === Qt.Checked)
                                    hasCheckedValues = true
                                if (val === Qt.Unchecked)
                                    hasUncheckedValues = true
                            }

                            if (hasCheckedValues && hasUncheckedValues)
                                headerButton.setCheckState(Qt.PartiallyChecked)
                            else if (hasCheckedValues)
                                headerButton.setCheckState(Qt.Checked)
                            else if (hasUncheckedValues)
                                headerButton.setCheckState(Qt.Unchecked)
                            else if (control.model.rowCount() === 0)
                                headerButton.setCheckState(Qt.Unchecked)
                            else
                                console.warn("Unexpected model check states")
                            control.headerCheckStateChanged()
                        }

                        model: control.model

                        Component.onCompleted: updateCheckState()

                        onHeaderDataChanged: (orientation, first, last) =>
                        {
                            if (index < first || index > last)
                                return

                            if (orientation !== Qt.Horizontal)
                                return

                            headerButton.text = model.headerData(index, Qt.Horizontal)
                        }

                        onDataChanged: updateCheckState()
                        onRowsRemoved: updateCheckState()
                        onRowsInserted: updateCheckState()
                    }

                    width: calculateWidth(control.columnWidthProvider(index))
                    height: columnsHeader.buttonHeight

                    visible: width > 0

                    text: control.model
                        ? control.model.headerData(index, Qt.Horizontal)
                        : ""

                    isCheckbox: isCheckboxColumn(control.model, index)

                    onCheckStateChanged: (checkState) =>
                    {
                        if (checkState === Qt.PartiallyChecked)
                            return

                        for (let row = 0; row < control.model.rowCount(); ++row)
                            headerDataAccessor.setData(row, index, checkState, "edit")
                    }

                    onClicked:
                    {
                        if (isCheckboxColumn(control.model, index))
                            return

                        for (let i = 0; i < repeater.count; ++i)
                        {
                            if (i !== index)
                                repeater.itemAt(i).sortOrder = HeaderButton.NoOrder
                        }

                        if (sortOrder === HeaderButton.AscendingOrder)
                            sortOrder = HeaderButton.DescendingOrder
                        else if (sortOrder === HeaderButton.DescendingOrder)
                            sortOrder = HeaderButton.AscendingOrder
                        else
                            sortOrder = HeaderButton.AscendingOrder

                        let modelSortOrder = sortOrder == HeaderButton.AscendingOrder
                            ? Qt.AscendingOrder
                            : Qt.DescendingOrder

                        control.model.sort(index, modelSortOrder)
                        control.forceLayout()
                    }
                }
            }
        }

        Rectangle
        {
            id: separator

            y: columnsHeader.buttonHeight

            width: control.width
            height: 1

            color: ColorTheme.colors.dark12
        }
    }
}

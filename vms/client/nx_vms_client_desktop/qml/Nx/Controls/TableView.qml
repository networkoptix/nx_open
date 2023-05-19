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

    property alias horizontalHeaderVisible: columnsHeader.visible
    property alias headerBackgroundColor: columnsHeader.color

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

    onWidthChanged: control.forceLayout()

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

                    property var headerDataAccessor: ModelDataAccessor
                    {
                        function getCheckState(columnIndex)
                        {
                            if (!control.model)
                                return Qt.Unchecked

                            let state = control.model.headerData(
                                index,
                                Qt.Horizontal,
                                Qt.CheckStateRole)

                            return state != null ? state : Qt.Unchecked
                        }

                        model: control.model

                        Component.onCompleted:
                        {
                            headerButton.setCheckState(getCheckState(index))
                        }

                        onHeaderDataChanged: (orientation, first, last) =>
                        {
                            if (index < first || index > last)
                                return

                            if (orientation !== Qt.Horizontal)
                                return

                            headerButton.setCheckState(getCheckState(index))
                        }
                    }

                    width: control.columnWidthProvider(index)
                    height: columnsHeader.buttonHeight

                    text: control.model
                        ? control.model.headerData(index, Qt.Horizontal)
                        : ""

                    isCheckbox: isCheckboxColumn(control.model, index)
                    onCheckStateChanged: (checkState) =>
                    {
                        headerDataAccessor.setHeaderData(
                            index,
                            Qt.Horizontal,
                            checkState,
                            Qt.CheckStateRole)
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

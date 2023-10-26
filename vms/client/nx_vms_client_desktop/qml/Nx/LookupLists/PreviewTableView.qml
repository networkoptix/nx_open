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

    property bool correct: true
    readonly property string needSelectionText: qsTr("Select attribute")
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

        // To avoid error in Nx.ComboBox, it doesn't accept QList<QString>
        // so have to implicitly convert to qml list of string.
        function getModel(index)
        {
            if (!control.model)
                return []

            var result = []
            var cppData = control.model.headerData(index, Qt.Horizontal)
            if (cppData)
            {
                for (let i = 0; i < cppData.length; ++i)
                    result.push(String(cppData[i]))
            }
            return result
        }

        CoreItems.Row
        {
            id: headerRow

            spacing: columnsHeader.comboBoxSpacing

            function columnWithoutSelectionExist()
            {
                for (let i = 0; i < repeater.count; ++i)
                {
                    if(!repeater.itemAt(i))
                        return false
                    if (repeater.itemAt(i).displayText === control.needSelectionText)
                        return true
                }
                return false
            }

            function reassignDependantColumns(changedColumnName, changedColumnIndex)
            {
                for (let i = 0; i < repeater.count; ++i)
                {
                    if (!repeater.itemAt(i))
                        return

                    if (changedColumnIndex === i)
                        continue

                    if (repeater.itemAt(i).displayText === changedColumnName &&
                        repeater.itemAt(i).displayText !== control.model.doNotImportText &&
                        repeater.itemAt(i).displayText !== control.needSelectionText)
                    {
                        repeater.itemAt(i).displayText = control.needSelectionText
                    }
                }
            }

            CoreItems.Repeater
            {
                id: repeater

                model: control.columns > 0 ? control.columns : 1

                ComboBox
                {
                    id: headerComboBox

                    width: columnsHeader.comboBoxWidth
                    height: columnsHeader.comboBoxHeight
                    visible: control.model.rowCount
                    model: columnsHeader.getModel(index)

                    onCurrentTextChanged:
                    {
                        displayText = currentText
                        control.model.headerIndexChanged(index, currentText)
                        headerRow.reassignDependantColumns(currentText, index)
                        control.correct = !headerRow.columnWithoutSelectionExist()
                    }

                    Connections
                    {
                        target: control.model

                        function onHeaderDataChanged()
                        {
                            model = columnsHeader.getModel(index)
                        }
                    }
                }
            }
        }
    }
}

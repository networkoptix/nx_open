// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Controls
import Nx.Core
import Nx.Dialogs

import nx.vms.client.desktop

Dialog
{
    id: control

    title: qsTr("Import From Devices")

    property alias model: tableView.model

    readonly property string filter: searchEdit.text

    width: 820
    height: 405
    minimumWidth: 400
    minimumHeight: 200

    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    contentItem: Rectangle
    {
        id: backgroundRect

        color: ColorTheme.colors.dark7

        Rectangle
        {
            id: searchRectangle

            width: parent.width
            height: 60

            color: ColorTheme.colors.dark8

            SearchField
            {
                id: searchEdit

                x: 16
                y: 16
                width: parent.width - x * 2

                onTextChanged:
                {
                    if (control.model)
                        control.model.setFilterRegularExpression(NxGlobals.escapeRegExp(text))
                }
            }
        }

        TableView
        {
            id: tableView

            readonly property var columnWidths: [290, 239, 239]

            readonly property var totalColumnsWidth:
                columnWidths[0] + columnWidths[1] + columnWidths[2]

            x: 26
            y: searchRectangle.y + searchRectangle.height + 18
            width: parent.width - x * 2
            height: parent.height - y - 26

            horizontalHeaderVisible: true

            columnWidthProvider: function (column)
            {
                if (tableView.columns == 0)
                    return 1

                return tableView.width / totalColumnsWidth * columnWidths[column]
            }

            delegate: CellDelegate {}
        }
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

TableView
{
    id: tableView

    property alias horizontalHeaderVisible: columnsHeader.visible

    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    topMargin: columnsHeader.visible ? columnsHeader.height : 0

    columnWidthProvider: function (column)
    {
        return tableView.width / (tableView.columns > 0 ? tableView.columns : 1)
    }

    ScrollBar.vertical: ScrollBar
    {
        parent: tableView.parent //< The only way to shift ScrollBar yCoord and change height.

        x: tableView.x + tableView.width - width
        y: tableView.y + (columnsHeader.visible ? columnsHeader.height : 0)
        height: tableView.height - (columnsHeader.visible ? columnsHeader.height : 0)
    }

    delegate: BasicTableCellDelegate
    {}

    onWidthChanged: tableView.forceLayout()

    Rectangle
    {
        id: columnsHeader

        readonly property int buttonHeight: 32

        y: tableView.contentY
        z: 2 //< Is needed for hiding scrolled rows.
        width: parent.width
        height: buttonHeight + separator.height + 8

        color: backgroundRect.color

        visible: false

        Row
        {
            Repeater
            {
                id: repeater

                model: tableView.columns > 0 ? tableView.columns : 1

                Button
                {
                    id: button

                    // We cannot use enum due to QtQuick.TableView namespace collision.
                    readonly property int kAscendingOrder: 0
                    readonly property int kDescendingOrder: 1
                    readonly property int kNoOrder: 2

                    property int sortOrder: kNoOrder

                    width: tableView.columnWidthProvider(modelData)
                    height: columnsHeader.buttonHeight

                    leftPadding: 8
                    rightPadding: 8

                    clip: true

                    backgroundColor: "transparent"
                    hoveredColor: ColorTheme.transparent(ColorTheme.colors.dark9, 0.2)
                    pressedColor: hoveredColor

                    textColor: ColorTheme.colors.light4
                    font.pixelSize: 14
                    font.weight: Font.Medium

                    text: control.model
                        ? control.model.headerData(modelData, Qt.Horizontal)
                        : ""

                    iconUrl:
                    {
                        if (sortOrder === kAscendingOrder)
                            return "qrc:///skin/table_view/ascending.svg"

                        if (sortOrder === kDescendingOrder)
                            return "qrc:///skin/table_view/descending.svg"

                        return ""
                    }

                    onClicked:
                    {
                        for (let i = 0; i < repeater.count; ++i)
                        {
                            if (i !== index)
                                repeater.itemAt(i).sortOrder = kNoOrder
                        }

                        if (sortOrder === kAscendingOrder)
                            sortOrder = kDescendingOrder
                        else if (sortOrder === kDescendingOrder)
                            sortOrder = kAscendingOrder
                        else
                            sortOrder = kAscendingOrder

                        tableView.model.sort(index, sortOrder)
                    }

                    contentItem: Item
                    {
                        Text
                        {
                            id: label

                            width: Math.min(implicitWidth, parent.width - iconItem.width - 4)
                            anchors.verticalCenter: parent.verticalCenter

                            text: button.text

                            color: button.textColor
                            font: button.font
                            elide: Text.ElideRight
                        }

                        IconImage
                        {
                            id: iconItem

                            anchors.left: label.right
                            anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter

                            source: button.icon.source
                            visible: source != "" && width > 0 && height > 0

                            width: source != "" ? button.icon.width : 0
                            height: source != "" ? button.icon.height : 0

                            color: button.textColor
                        }
                    }
                }
            }
        }

        Rectangle
        {
            id: separator

            y: columnsHeader.buttonHeight

            width: tableView.width
            height: 1

            color: ColorTheme.colors.dark12
        }
    }
}

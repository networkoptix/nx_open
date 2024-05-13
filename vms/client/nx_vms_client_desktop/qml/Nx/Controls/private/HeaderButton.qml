// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick

import Nx
import Nx.Controls
import Nx.Core
import Nx.Models

import nx.vms.client.core
import nx.vms.client.desktop

Button
{
    id: control

    property AbstractItemModel model //< The property is not marked as required intentionally, to prevent assignment from the wrong context.

    property var headerDataAccessor: ModelDataAccessor
    {
        model: control.model

        onHeaderDataChanged: (orientation, first, last) =>
        {
            if (index < first || index > last)
                return

            if (orientation !== Qt.Horizontal)
                return

            control.text = model.headerData(index, Qt.Horizontal)
        }
    }

    leftPadding: 8
    rightPadding: 8

    clip: true

    backgroundColor: "transparent"
    hoveredColor: ColorTheme.transparent(ColorTheme.colors.dark9, 0.2)
    pressedColor: hoveredColor

    textColor: ColorTheme.colors.light4
    font.pixelSize: 14
    font.weight: Font.Medium

    text: !!model ? model.headerData(index, Qt.Horizontal) : ""

    icon.source:
    {
        if (!!model && model.sortColumn === index)
        {
            if (model.sortOrder == Qt.AscendingOrder)
                return "image://skin/16x16/Solid/sorting_ascending.svg"

            if (model.sortOrder == Qt.DescendingOrder)
                return "image://skin/16x16/Solid/sorting_descending.svg"
        }

        return ""
    }

    contentItem: Item
    {
        implicitWidth:
            label.implicitWidth + iconItem.anchors.leftMargin + iconItem.implicitWidth

        implicitHeight: Math.max(label.height, iconItem.height)

        Text
        {
            id: label

            width: Math.min(implicitWidth, parent.width - iconItem.width - 4)
            anchors.verticalCenter: parent.verticalCenter

            text: control.text

            color: control.textColor
            font: control.font
            elide: Text.ElideRight
        }

        ColoredImage
        {
            id: iconItem

            anchors.left: label.right
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter

            sourcePath: control.icon.source
            sourceSize: Qt.size(control.icon.width, control.icon.height)
            visible: !!sourcePath && width > 0 && height > 0

            width: sourcePath !== "" ? control.icon.width : 0
            height: sourcePath !== "" ? control.icon.height : 0

            primaryColor: control.textColor
        }
    }

    onClicked:
    {
        if (!model)
            return

        let sortOrder = Qt.AscendingOrder
        if (model.sortColumn === index)
        {
            if (model.sortOrder === Qt.AscendingOrder)
                sortOrder = Qt.DescendingOrder
            else
                sortOrder = Qt.AscendingOrder
        }

        control.model.sort(index, sortOrder)
    }
}

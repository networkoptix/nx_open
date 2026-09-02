// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core

BaseAdaptiveSheet
{
    id: control

    property alias title: titleTextItem.text
    property alias titleTextItem: titleTextItem
    property alias titleCustomArea: customArea
    property alias contentSpacing: contentColumn.spacing
    default property alias sheetData: contentColumn.data

    spacing: 14

    header: RowLayout
    {
        spacing: 8

        Text
        {
            id: titleTextItem

            font.pixelSize: 18
            font.weight: 500
            color: ColorTheme.colors.light4
            wrapMode: Text.Wrap
        }

        Item
        {
            id: customArea

            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    Column
    {
        id: contentColumn

        spacing: 8
        width: parent.width
    }
}

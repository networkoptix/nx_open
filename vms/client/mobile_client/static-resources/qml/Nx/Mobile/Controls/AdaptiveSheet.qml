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
    default property alias data: contentColumn.data

    spacing: 14

    header: RowLayout
    {
        spacing: 8

        Text
        {
            id: titleTextItem

            Layout.fillWidth: true
            font.pixelSize: 24
            font.weight: 500
            color: ColorTheme.colors.light4
            wrapMode: Text.Wrap
        }

        Row
        {
            id: customArea
        }
    }

    Column
    {
        id: contentColumn

        spacing: 8
        width: parent.width
    }
}

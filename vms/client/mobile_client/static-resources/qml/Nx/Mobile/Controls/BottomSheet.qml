// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Mobile.Controls

BaseBottomSheet
{
    id: control

    property alias title: titleTextItem.text
    property alias titleTextItem: titleTextItem
    property alias titleCustomArea: customArea
    property alias contentSpacing: contentColumn.spacing
    default property alias data: contentColumn.data

    spacing: 14

    Row
    {
        spacing: 8
        width: parent.width

        Text
        {
            id: titleTextItem

            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - customArea.width - spacing
            font.pixelSize: 24
            font.weight: 500
            color: ColorTheme.colors.light4
            wrapMode: Text.Wrap
        }

        Row
        {
            id: customArea

            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Column
    {
        id: contentColumn

        spacing: 24
        width: parent.width
    }
}

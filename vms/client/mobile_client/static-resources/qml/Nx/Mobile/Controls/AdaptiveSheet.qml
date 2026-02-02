// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Mobile.Controls
import Nx.Ui

BaseAdaptiveSheet
{
    id: control

    property alias title: titleTextItem.text
    property alias titleTextItem: titleTextItem
    property alias titleCustomArea: customArea
    property alias contentSpacing: contentColumn.spacing
    default property alias data: contentColumn.data

    spacing: 14

    RowLayout
    {
        spacing: 8
        width: parent.width

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

        IconButton
        {
            icon.source: "image://skin/24x24/Outline/close.svg?primary=light10"
            icon.width: 24
            icon.height: 24
            visible: LayoutController.isTabletLayout
            onClicked: control.close()
        }
    }

    Column
    {
        id: contentColumn

        spacing: 8
        width: parent.width
    }
}

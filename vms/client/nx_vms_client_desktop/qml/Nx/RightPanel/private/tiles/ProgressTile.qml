// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import nx.vms.client.core
import nx.vms.client.desktop

TileBase
{
    id: tile

    contentItem: Item
    {
        implicitWidth: contentLayout.implicitWidth
        implicitHeight: contentLayout.implicitHeight
        clip: true

        ColumnLayout
        {
            id: contentLayout

            spacing: 8
            width: parent.width

            ProgressBar
            {
                id: progress

                Layout.fillWidth: true
                from: 0
                to: 1

                value: (model && model.progressValue) || 0
                title: (model && model.display) || ""

                font { pixelSize: FontConfig.normal.pixelSize; weight: Font.Medium }
                percentageFont.pixelSize: FontConfig.small.pixelSize
            }

            Text
            {
                id: description

                wrapMode: Text.Wrap
                visible: text.length
                color: tile.secondaryForegroundColor
                font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }

                text: (model && model.description) || ""
            }
        }
    }
}

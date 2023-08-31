// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Layouts 1.11

import Nx.Controls 1.0

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

                font { pixelSize: 13; weight: Font.DemiBold }
                percentageFont.pixelSize: 11
            }

            Text
            {
                id: description

                wrapMode: Text.Wrap
                visible: text.length
                color: tile.secondaryForegroundColor
                font { pixelSize: 11; weight: Font.Normal }

                text: (model && model.description) || ""
            }
        }
    }
}

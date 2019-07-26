import QtQuick 2.6
import QtQuick.Controls.impl 2.4
import QtQuick.Layouts 1.11

import "private"

TileBase
{
    id: tile

    contentItem: ColumnLayout
    {
        spacing: 4

        Layout.topMargin: 0

        RowLayout
        {
            spacing: 8

            ResourceList
            {
                id: resourceList
                resourceNames: model && model.resourceList
            }

            Text
            {
                id: timestamp

                Layout.minimumWidth: implicitWidth
                Layout.alignment: Qt.AlignRight | Qt.AlignTop

                color: tile.palette.windowText
                visible: text.length && !tile.hovered
                font { pixelSize: 11; weight: Font.Normal }

                text: (model && model.timestamp) || ""
            }
        }
    }
}

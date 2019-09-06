import QtQuick 2.6
import QtQuick.Controls.impl 2.4
import QtQuick.Layouts 1.11

import "private"

TileBase
{
    id: tile

    contentItem: ColumnLayout
    {
        spacing: 6

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
                visible: text.length
                font { pixelSize: 11; weight: Font.Normal }

                text: (model && model.timestamp) || ""
            }
        }

        Preview
        {
            id: preview

            Layout.fillWidth: true

            previewId: (model && model.previewId) || ""
            previewState: (model && model.previewState) || 0
            previewAspectRatio: (model && model.previewAspectRatio) || 1
            visible: (model && model.previewResource) || false
        }
    }
}

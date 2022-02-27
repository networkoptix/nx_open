// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls.impl 2.4
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Controls 1.0

import ".."
import "../../globals.js" as RightPanelGlobals

TileBase
{
    id: tile

    contentItem: SimpleColumn
    {
        id: tileContent

        clip: true
        spacing: 6

        RowLayout
        {
            spacing: 8
            width: tileContent.width

            ResourceList
            {
                id: resourceList
                resourceNames: (model && model.resourceList) || []
            }

            Text
            {
                id: timestamp

                Layout.minimumWidth: implicitWidth
                Layout.alignment: Qt.AlignRight | Qt.AlignTop

                color: tile.palette.windowText
                visible: !!text
                font { pixelSize: 11; weight: Font.Normal }

                text: (model && model.timestamp) || ""
            }
        }

        Preview
        {
            id: preview

            width: tileContent.width

            previewId: (model && model.previewId) || ""
            previewState: (model && model.previewState) || 0
            previewAspectRatio: (model && model.previewAspectRatio) || 1
            highlightRect: (model && model.previewHighlightRect) || Qt.rect(0, 0, 0, 0)

            visible: (model && model.previewResource
                && (!tile.controller || tile.controller.showThumbnails))
                    || false

            videoPreviewEnabled: tile.controller
                && tile.controller.videoPreviewMode != RightPanelGlobals.VideoPreviewMode.none
                && (tile.controller.videoPreviewMode != RightPanelGlobals.VideoPreviewMode.selection
                    || tile.controller.selectedRow === index)

            videoPreviewForced: tile.controller
                && tile.controller.videoPreviewMode == RightPanelGlobals.VideoPreviewMode.selection
                && tile.controller.selectedRow === index

            videoPreviewTimestampMs: (model && model.previewTimestampMs) || 0

            videoPreviewResourceId: (model && model.previewResource && model.previewResource.id)
                || NxGlobals.uuid("")
        }
    }
}

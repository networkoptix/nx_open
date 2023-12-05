// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

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
                color: tile.foregroundColor
                remainderColor: tile.secondaryForegroundColor
            }

            Text
            {
                id: timestamp

                Layout.minimumWidth: implicitWidth
                Layout.alignment: Qt.AlignRight | Qt.AlignTop

                color: tile.secondaryForegroundColor
                visible: !!text
                font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }

                text: (model && model.textTimestamp) || ""
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

            foregroundColor: tile.secondaryForegroundColor

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

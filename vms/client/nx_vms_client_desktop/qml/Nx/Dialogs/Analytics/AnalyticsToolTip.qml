// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

BubbleToolTip
{
    id: toolTip

    property alias previewState: preview.previewState
    property alias videoPreviewTimestampMs: preview.videoPreviewTimestampMs
    property alias videoPreviewResource: preview.videoPreviewResource
    property alias attributeItems: attributeTable.items
    property alias previewAspectRatio: preview.previewAspectRatio
    property alias footerText: footer.text

    minIntersection: 64

    contentItem: Item
    {
        id: content

        implicitWidth: 240
        implicitHeight: column.implicitHeight

        Column
        {
            id: column

            spacing: 8
            width: content.width

            AnalyticsToolTipPreview
            {
                id: preview
                width: content.width
            }

            NameValueTable
            {
                id: attributeTable

                width: content.width
                visible: items.length

                nameColor: ColorTheme.colors.light10
                valueColor: ColorTheme.colors.light16
                nameFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
                valueFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Medium }
            }

            Rectangle
            {
                width: content.width
                height: 1
                color: ColorTheme.colors.dark12
                visible: attributeTable.visible && footer.visible
            }

            Text
            {
                id: footer

                width: content.width
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.RichText
                visible: !!text

                color: ColorTheme.colors.light16
                font { pixelSize: 11; weight: Font.Normal }
                wrapMode: Text.Wrap
            }
        }
    }
}

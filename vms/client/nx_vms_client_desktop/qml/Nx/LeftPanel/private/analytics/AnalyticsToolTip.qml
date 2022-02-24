// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.5

import Nx 1.0
import Nx.Controls 1.0
import Nx.Core 1.0
import Nx.Items 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

BubbleToolTip
{
    id: toolTip

    property alias previewState: preview.previewState
    property alias videoPreviewTimestampMs: preview.videoPreviewTimestampMs
    property alias videoPreviewResourceId: preview.videoPreviewResourceId
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

                nameColor: ColorTheme.colors.light13
                valueColor: ColorTheme.colors.light7
                nameFont { pixelSize: 11; weight: Font.Normal }
                valueFont { pixelSize: 11; weight: Font.Medium }
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

                color: ColorTheme.colors.light13
                font { pixelSize: 11; weight: Font.Normal }
                wrapMode: Text.Wrap
            }
        }
    }
}

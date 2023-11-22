// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Controls
import Nx.Effects
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

Bubble
{
    id: toolTip

    property alias previewSource: thumbnail.previewSource
    property alias previewState: thumbnail.previewState
    property alias previewAspectRatio: thumbnail.previewAspectRatio
    property alias attributes: attributeTable.items

    property bool thumbnailVisible: false
    property alias thumbnailHighlightRect: thumbnail.highlightRect

    property size maximumContentSize: Qt.size(600, 400) //< Some sensible default.

    contentItem: Item
    {
        implicitWidth: column.width
        implicitHeight: column.height

        Column
        {
            id: column

            layer.effect: EdgeOpacityGradient { edges: Qt.BottomEdge }
            layer.enabled: height < implicitHeight

            spacing: 2
            height: Math.min(implicitHeight, toolTip.maximumContentSize.height)

            Preview
            {
                id: thumbnail

                implicitWidth: 224
                minimumAspectRatio: 0.25
                visible: toolTip.thumbnailVisible
                showHighlightBorder: true
            }

            TextEdit
            {
                id: description

                readOnly: true
                cursorVisible: false
                wrapMode: Text.Wrap
                color: ColorTheme.colors.brand_contrast
                textFormat: NxGlobals.mightBeHtml(toolTip.text) ? Qt.RichText : Qt.PlainText
                text: toolTip.text

                width: toolTip.thumbnailVisible
                    ? thumbnail.width
                    : Math.min(implicitWidth, toolTip.maximumContentSize.width)
            }

            NameValueTable
            {
                id: attributeTable

                visible: items.length > 0

                width: toolTip.thumbnailVisible
                    ? thumbnail.width
                    : Math.min(implicitWidth, toolTip.maximumContentSize.width)

                nameColor: ColorTheme.colors.light13
                valueColor: ColorTheme.colors.light7
                nameFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
                valueFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Medium }
            }
        }
    }
}

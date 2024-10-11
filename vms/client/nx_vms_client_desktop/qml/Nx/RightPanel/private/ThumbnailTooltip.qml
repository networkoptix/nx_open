// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
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

    color: ColorTheme.colors.dark10

    contentItem: Item
    {
        id: content

        implicitWidth:
        {
            if (toolTip.thumbnailVisible)
                return 224

            let result = description.relevant ? description.implicitWidth : 0

            if (attributeTable.relevant)
                result = Math.max(result, attributeTable.implicitWidth)

            return result
        }

        implicitHeight:
        {
            let heights = []

            if (toolTip.thumbnailVisible)
                heights.push(thumbnail.implicitHeight)

            if (description.relevant)
                heights.push(description.implicitHeight)

            if (attributeTable.relevant)
                heights.push(attributeTable.implicitHeight)

            if (!heights.length)
                return 0

            let result = heights.reduce((a, b) => a + b) + (heights.length - 1) * column.spacing
            return Math.min(result, toolTip.maximumContentSize.height)
        }

        Column
        {
            id: column

            layer.effect: EdgeOpacityGradient { edges: Qt.BottomEdge }
            layer.enabled: height < implicitHeight

            spacing: 2
            anchors.fill: parent

            Preview
            {
                id: thumbnail

                implicitWidth: content.width
                minimumAspectRatio: 0.25
                visible: toolTip.thumbnailVisible
                showHighlightBorder: true
            }

            MultilineText
            {
                id: description

                readonly property bool relevant: !!text
                visible: relevant

                width: content.width

                height:
                {
                    let availableHeight = content.height

                    if (toolTip.thumbnailVisible)
                        availableHeight -= thumbnail.height + column.spacing

                    if (attributeTable.relevant)
                        availableHeight -= attributeTable.height + column.spacing

                    return Math.min(implicitHeight, availableHeight)
                }

                wrapMode: Text.Wrap
                color: ColorTheme.colors.light16
                text: toolTip.text
            }

            NameValueTable
            {
                id: attributeTable

                readonly property bool relevant: items.length > 0
                visible: relevant

                width: content.width

                nameColor: ColorTheme.colors.light10
                valueColor: ColorTheme.colors.light16
                nameFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
                valueFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Medium }
            }
        }
    }
}

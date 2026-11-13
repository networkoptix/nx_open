// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Analytics
import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Core.Effects
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

Bubble
{
    id: toolTip

    property alias previewSource: thumbnail.previewSource
    property alias previewState: thumbnail.previewState
    property alias previewAspectRatio: thumbnail.previewAspectRatio
    property alias attributes: attributeTable.attributes

    property bool thumbnailVisible: false
    property alias thumbnailHighlightRect: thumbnail.highlightRect

    // Labels over image areas: [{rect: <area in fractions of the image size>, text: <string>}].
    property var imageLabels: []

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

                Item
                {
                    id: paintedImageArea

                    z: 1

                    // The image is fit into the preview content keeping its aspect ratio.
                    readonly property real aspectRatio: thumbnail.previewAspectRatio || 1

                    width: Math.min(thumbnail.availableWidth,
                        thumbnail.availableHeight * aspectRatio)
                    height: width / aspectRatio
                    x: thumbnail.leftPadding + (thumbnail.availableWidth - width) / 2
                    y: thumbnail.topPadding + (thumbnail.availableHeight - height) / 2

                    Repeater
                    {
                        model: toolTip.imageLabels

                        // Keep in sync with the preview counter style in EventTile.
                        Rectangle
                        {
                            readonly property rect labelRect: modelData.rect
                            readonly property real inset: 4
                            readonly property real labelRight:
                                (labelRect.x + labelRect.width) * paintedImageArea.width
                            readonly property real labelBottom:
                                (labelRect.y + labelRect.height) * paintedImageArea.height

                            x: Math.round(labelRight - width - inset)
                            y: Math.round(labelBottom - height - inset)

                            width: counterText.implicitWidth + 8
                            height: 20
                            radius: 2
                            color: ColorTheme.transparent(ColorTheme.colors.dark6, 0.9)

                            Text
                            {
                                id: counterText

                                anchors.centerIn: parent
                                text: modelData.text
                                color: ColorTheme.colors.light4
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                font.letterSpacing: 0.4
                            }
                        }
                    }
                }
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

            AnalyticsAttributeTable
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

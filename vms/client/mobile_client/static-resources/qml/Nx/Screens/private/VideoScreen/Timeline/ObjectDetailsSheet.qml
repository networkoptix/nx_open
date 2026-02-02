// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Mobile.Controls

import nx.vms.client.core

BaseAdaptiveSheet
{
    id: sheet

    property var model

    // Presumably loaded low-resolution images from the tile.
    property var fallbackImagePaths: []

    readonly property alias count: swipeView.count
    property alias currentIndex: swipeView.currentIndex

    property var dateFormatter: ((timestampMs) => new Date(timestampMs).toLocaleString())

    spacing: 16
    extraBottomPadding: (pageNavigationBar.count > 1) ? 0 : 20

    SwipeView
    {
        id: swipeView

        x: -parent.leftPadding
        width: parent.width + parent.leftPadding + parent.rightPadding
        height: contentChildren.reduce((h, child) => Math.max(h, child.implicitHeight), 0)

        onCurrentIndexChanged:
            pageNavigationBar.index = currentIndex

        Repeater
        {
            id: repeater

            Column
            {
                id: objectInfoColumn

                readonly property real contentWidth: width - leftPadding - rightPadding

                leftPadding: swipeView.parent.leftPadding
                rightPadding: swipeView.parent.rightPadding

                width: swipeView.width
                spacing: 16

                RemoteImage
                {
                    id: image

                    width: objectInfoColumn.contentWidth
                    height: width * (9.0 / 16.0)

                    requestLine: modelData?.imagePath ?? ""

                    // Presumably loaded low-resolution image from the tile.
                    fallbackImagePath:
                    {
                        const reverseIndex = swipeView.count - objectInfoColumn.SwipeView.index - 1
                        const imagePath = sheet.fallbackImagePaths?.[reverseIndex]
                        return imagePath ? `image://remote/${imagePath}` : ""
                    }

                    fallbackImageIfError: true
                }

                Column
                {
                    id: textBlocks

                    spacing: 4
                    width: objectInfoColumn.contentWidth

                    Text
                    {
                        id: timestamp

                        font.pixelSize: 12
                        color: ColorTheme.colors.light16
                        width: parent.width
                        wrapMode: Text.Wrap
                        visible: !!text

                        text:
                        {
                            if (!modelData)
                                return ""

                            const duration = Duration.toString(modelData.durationMs,
                                Duration.Hours | Duration.Minutes | Duration.Seconds,
                                Duration.Short)

                            const date = sheet.dateFormatter
                                ? sheet.dateFormatter(modelData.startTimeMs)
                                : new Date(modelData.startTimeMs).toLocaleString()

                            return `${date} (${duration})`
                        }
                    }

                    Text
                    {
                        id: caption

                        font.pixelSize: 24
                        font.weight: Font.Medium
                        color: ColorTheme.colors.light4
                        text: modelData?.title ?? ""
                        width: parent.width
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        bottomPadding: 2
                        visible: !!text
                    }

                    Text
                    {
                        id: description

                        font.pixelSize: 16
                        font.weight: Font.Normal
                        color: ColorTheme.colors.light10
                        text: modelData?.description ?? ""
                        width: parent.width
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        visible: !!text
                    }
                }

                TagView
                {
                    id: tagView

                    width: objectInfoColumn.contentWidth
                    model: modelData?.tags ?? []
                }

                Rectangle
                {
                    height: 1
                    width: objectInfoColumn.contentWidth
                    color: ColorTheme.colors.dark12
                    visible: !!attributeTable.attributes.length
                }

                AnalyticsAttributeTable
                {
                    id: attributeTable

                    width: objectInfoColumn.contentWidth

                    attributes: modelData?.attributes ?? []
                    visible: !!attributes.length

                    nameFont.pixelSize: 14
                    nameColor: ColorTheme.colors.light10
                    valueFont.pixelSize: 14
                    valueColor: ColorTheme.colors.light7
                    valueFont.weight: Font.Medium
                    colorBoxSize: 16
                    rowSpacing: 4
                }
            }
        }
    }

    PageNavigationBar
    {
        id: pageNavigationBar

        x: swipeView.x
        width: swipeView.width

        count: sheet.model?.length ?? 0
        visible: count > 1

        onIndexChanged:
            swipeView.setCurrentIndex(index)
    }

    onModelChanged:
    {
        repeater.model = sheet.model ?? []
        swipeView.setCurrentIndex(swipeView.count ? 0 : -1)
    }
}

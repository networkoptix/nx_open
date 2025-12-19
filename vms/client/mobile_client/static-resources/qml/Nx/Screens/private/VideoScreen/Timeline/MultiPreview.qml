// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

// A specialized preview for ObjectsListDelegate, displaying up to 4 images in a 2x2 grid.
Rectangle
{
    id: multiPreview

    property int totalCount: 0
    property var paths: []

    property int maxCountToDisplay: 100

    readonly property int imageCount: (totalCount > 0) ? ((totalCount <= 4) ? totalCount : 3) : 0
    readonly property int remainder: (totalCount > 4) ? (totalCount - 3) : 0

    color: ColorTheme.colors.mobileTimeline.tile.preview.multiBackground
    radius: 6

    Grid
    {
        id: grid

        columns: 2
        spacing: 4
        padding: 4

        anchors.fill: multiPreview

        readonly property real availableWidth: width - leftPadding - rightPadding
        readonly property real availableHeight: height - topPadding - bottomPadding

        // Each item width and height, for 2x2 configuration.
        readonly property real itemWidth: Math.round((availableWidth - spacing) * 0.5)
        readonly property real itemHeight: Math.round((availableHeight - spacing) * 0.5)

        Repeater
        {
            model: multiPreview.imageCount

            RemoteImage
            {
                requestLine: multiPreview.paths?.[index] ?? ""

                width: grid.itemWidth
                height: grid.itemHeight

                roundingRadius: 2
                frameColor: "transparent"

                preloader.dotRadius: 2
                preloader.spacing: 3
                font.pixelSize: 8

                backgroundColor:
                    ColorTheme.colors.mobileTimeline.tile.preview.noDataBackground

                foregroundColor:
                    ColorTheme.colors.mobileTimeline.tile.preview.noDataText
            }
        }

        Rectangle
        {
            id: plusMore

            width: grid.itemWidth
            height: grid.itemHeight

            visible: multiPreview.remainder > 0
            color: ColorTheme.colors.mobileTimeline.tile.preview.plusMoreBackground
            radius: 2

            Text
            {
                anchors.centerIn: parent

                font.pixelSize: 14
                font.weight: Font.Medium
                color: ColorTheme.colors.mobileTimeline.tile.preview.plusMoreText

                text: (multiPreview.totalCount > multiPreview.maxCountToDisplay)
                    ? `>${multiPreview.maxCountToDisplay}`
                    : `+${multiPreview.remainder}`
            }
        }
    }
}

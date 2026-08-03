// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Controls
import Nx.Items

import nx.vms.client.mobile.timeline as Timeline

Item
{
    id: tile

    property int objectCount: 0
    property bool tightGroup: false //< If objects' spread is too small and you can't zoom into.
    readonly property bool isStack: tile.objectCount > 1 && !tile.tightGroup
    readonly property bool isSkeleton: tile.objectCount === 0

    property alias caption: captionText.text
    property alias description: descriptionText.text
    property var imagePaths: []
    property var iconPaths: []

    property int objectsType: Timeline.ObjectsLoader.ObjectsType.motion //< Determines icon color.
    property bool highlighted: false
    property bool showPointer: true
    property real pointerY: 0 //< Relative to the tile top.
    property alias maxCountToDisplay: multiPreview.maxCountToDisplay
    property alias skeletonController: skeleton.controller

    Rectangle
    {
        id: background

        radius: 6
        visible: !tile.isSkeleton
        height: tile.height
        anchors.left: tile.left
        anchors.right: tile.right
        anchors.rightMargin: (tile.isStack || tile.showPointer) ? 10 : 0

        color: !tile.isStack && tile.highlighted
            ? ColorTheme.colors.mobileTimeline.tile.backgroundHighlight
            : ColorTheme.colors.mobileTimeline.tile.background

        Behavior on color { ColorAnimation { duration: 150 }}
    }

    Skeleton
    {
        id: skeleton

        anchors.fill: background
        visible: tile.isSkeleton

        color: ColorTheme.colors.mobileTimeline.tile.background2
        fillerColor: ColorTheme.lighter(color, 2)

        Rectangle
        {
            anchors.fill: parent
            radius: background.radius
        }
    }

    Item
    {
        id: backgroundSideElements

        anchors.left: background.right
        z: -1

        visible: !tile.isSkeleton

        Rectangle
        {
            id: stackBackground1

            anchors.left: backgroundSideElements.right
            visible: tile.isStack
            radius: 4
            color: ColorTheme.colors.mobileTimeline.tile.background3
            y: radius * 2
            height: background.height - y * 2
            width: radius * 2
        }

        Rectangle
        {
            id: stackBackground2

            anchors.left: backgroundSideElements.right
            anchors.leftMargin: radius - width
            visible: tile.isStack
            radius: 4
            color: ColorTheme.colors.mobileTimeline.tile.background2
            y: radius
            width: 16
            height: background.height - y * 2
        }

        Shape
        {
            id: tilePointer

            readonly property real length: 8
            readonly property real base: 8

            transform: Scale { xScale: tile.LayoutMirroring.enabled ? -1 : 1 }

            y: tile.pointerY - backgroundSideElements.y
            visible: !tile.isStack && tile.showPointer

            ShapePath
            {
                fillColor: background.color
                strokeColor: "transparent"
                joinStyle: ShapePath.RoundJoin
                pathHints: ShapePath.PathConvex | ShapePath.PathFillOnRight

                PathPolyline
                {
                    path:
                    {
                        if (tile.isStack)
                            return []

                        const halfWidth = tilePointer.base / 2

                        // Slanted pointer at the top of the tile.
                        if (tilePointer.y < background.radius + halfWidth)
                        {
                            const shift = -Math.min(tilePointer.y, 0)

                            return [
                                Qt.point(tilePointer.length, shift),
                                Qt.point(0, tilePointer.base + shift),
                                Qt.point(-background.radius, shift),
                                Qt.point(tilePointer.length, shift) ]
                        }

                        // Slanted pointer at the bottom of the tile.
                        if (tilePointer.y >= background.height
                            - background.radius - halfWidth)
                        {
                            const shift = -Math.max(0,
                                tilePointer.y - background.height + 1)

                            return [
                                Qt.point(tilePointer.length, shift),
                                Qt.point(-background.radius, shift),
                                Qt.point(0, -tilePointer.base + shift),
                                Qt.point(tilePointer.length, shift) ]
                        }

                        // Normal pointer in the middle of the tile.
                        return [
                            Qt.point(tilePointer.length, 0),
                            Qt.point(0, halfWidth),
                            Qt.point(0, -halfWidth),
                            Qt.point(tilePointer.length, 0) ]
                    }
                }
            }
        }
    }

    Item
    {
        id: delegateContent

        x: background.x + 8
        y: 8
        width: background.width - 16
        height: background.height - 16
        visible: !tile.isSkeleton
        clip: true

        Item
        {
            id: preview

            anchors.left: delegateContent.left
            width: delegateContent.width / 2
            height: delegateContent.height

            RemoteImage
            {
                id: singlePreview

                anchors.fill: preview
                visible: tile.objectCount === 1
                requestLine: (tile.objectCount === 1) ? (tile.imagePaths?.[0] ?? "") : ""

                frameColor: "transparent"
                backgroundColor: ColorTheme.colors.mobileTimeline.tile.preview.noDataBackground
                foregroundColor: ColorTheme.colors.mobileTimeline.tile.preview.noDataText
            }

            MultiPreview
            {
                id: multiPreview

                anchors.fill: preview
                visible: tile.objectCount > 1

                totalCount: (tile.objectCount > 1) ? tile.objectCount : 0
                paths: tile.imagePaths ?? []
            }
        }

        Item
        {
            id: info

            anchors.left: preview.right
            anchors.leftMargin: 12
            anchors.right: delegateContent.right
            y: 4
            height: delegateContent.height - y

            Row
            {
                id: iconRow

                LayoutMirroring.enabled: false

                topPadding: 4
                spacing: 8

                Repeater
                {
                    // Up to 3 icons are drawn.
                    model: (tile.iconPaths ?? []).slice(0, 3)

                    ColoredImage
                    {
                        sourceSize: Qt.size(20, 20)
                        sourcePath: modelData

                        primaryColor:
                        {
                            switch (tile.objectsType)
                            {
                                case Timeline.ObjectsLoader.ObjectsType.motion:
                                    return ColorTheme.colors.mobileTimeline.chunks.motion

                                case Timeline.ObjectsLoader.ObjectsType.analytics:
                                    return ColorTheme.colors.mobileTimeline.chunks.analytics

                                case Timeline.ObjectsLoader.ObjectsType.bookmarks:
                                    return ColorTheme.colors.mobileTimeline.chunks.bookmarks
                            }

                            return undefined
                        }
                    }
                }
            }

            Column
            {
                anchors.bottom: info.bottom

                Text
                {
                    id: captionText

                    readonly property bool scaledToFit: tile.objectCount > 1

                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: ColorTheme.colors.mobileTimeline.tile.caption
                    width: scaledToFit ? implicitWidth : info.width
                    elide: scaledToFit ? Text.ElideNone : Text.ElideRight

                    transform: Scale
                    {
                        xScale: captionText.scaledToFit
                            ? Math.min(1.0, info.width / captionText.implicitWidth)
                            : 1.0
                    }
                }

                Text
                {
                    id: descriptionText

                    font.pixelSize: 12
                    font.weight: Font.Normal
                    color: ColorTheme.colors.mobileTimeline.tile.description
                    width: info.width
                    elide: Text.ElideRight
                    textFormat: Text.StyledText
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    visible: !!text
                }
            }
        }
    }
}

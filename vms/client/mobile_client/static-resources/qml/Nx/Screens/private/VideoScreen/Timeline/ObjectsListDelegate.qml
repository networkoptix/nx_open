// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.mobile.timeline as Timeline

Item
{
    id: delegate

    // Context properties:
    //  var modelData
    //  ObjectsList objectsList

    readonly property bool isStack: modelData?.count > 1
        && modelData.durationMs >= objectsList.minimumStackDurationMs

    Rectangle
    {
        id: background

        radius: 6
        color: ColorTheme.colors.mobileTimeline.tile.background
        height: delegate.height
        anchors.left: delegate.left
        anchors.right: delegate.right
        anchors.rightMargin: 10
    }

    Item
    {
        id: backgroundSideElements

        anchors.left: background.right
        z: -1

        Rectangle
        {
            id: stackBackground1

            anchors.left: backgroundSideElements.right
            visible: delegate.isStack
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
            visible: delegate.isStack
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

            transform: Scale { xScale: delegate.LayoutMirroring.enabled ? -1 : 1 }

            y: background.mapFromItem(objectsList, 0,
                objectsList.timeToPosition(modelData?.positionMs ?? 0)).y

            visible: !delegate.isStack

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
                        if (delegate.isStack)
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
                visible: !delegate.isStack
                requestLine: delegate.isStack ? "" : (modelData?.imagePaths?.[0] ?? "")

                frameColor: "transparent"
                backgroundColor: ColorTheme.colors.mobileTimeline.tile.preview.noDataBackground
                foregroundColor: ColorTheme.colors.mobileTimeline.tile.preview.noDataText
            }

            MultiPreview
            {
                id: multiPreview

                anchors.fill: preview
                visible: delegate.isStack

                maxCountToDisplay: objectsList.maxObjectsPerBucket
                totalCount: delegate.isStack ? modelData?.count : 0
                paths: modelData?.imagePaths ?? []
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
                    model: (modelData?.iconPaths || []).slice(0, 3)

                    ColoredImage
                    {
                        sourceSize: Qt.size(20, 20)
                        sourcePath: modelData

                        primaryColor:
                        {
                            switch (objectsList.objectsType)
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
                    id: caption

                    readonly property bool scaledToFit: modelData?.count > 1

                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: ColorTheme.colors.mobileTimeline.tile.caption
                    text: modelData?.caption ?? ""
                    width: scaledToFit ? implicitWidth : info.width
                    elide: scaledToFit ? Text.ElideNone : Text.ElideRight

                    transform: Scale
                    {
                        xScale: caption.scaledToFit
                            ? Math.min(1.0, info.width / caption.implicitWidth)
                            : 1.0
                    }
                }

                Text
                {
                    id: description

                    font.pixelSize: 12
                    font.weight: Font.Normal
                    color: ColorTheme.colors.mobileTimeline.tile.description
                    text: modelData?.description ?? ""
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

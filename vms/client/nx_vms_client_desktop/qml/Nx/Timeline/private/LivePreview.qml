// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

TimeMarker
{
    id: livePreview

    property alias previewSource: resourcePreview.source
    property real desiredPreviewHeight: 0
    property real maximumPreviewWidth: 400 //< Sensible default.
    property real maximumPreviewHeight: 196
    property real minimumPreviewHeight: 54

    property color baseColor: ColorTheme.colors.dark17
    property real markerLineLength: 0

    display: { return Timeline.TimeMarkerDisplay.automatic }

    color: ColorTheme.transparent(baseColor, 0.8)
    textColor: ColorTheme.colors.light1
    font.pixelSize: FontConfig.small.pixelSize

    topOverhang: previewHolder.height
    bottomOverhang: markerLineLength

    implicitContentWidth: Math.max(contentItem.implicitWidth, previewHolder.width)

    Rectangle
    {
        id: previewHolder

        readonly property size desiredSize:
        {
            let width = livePreview.contentItem.implicitWidth
            let height = Math.max(
                width / resourcePreview.effectiveAspectRatio,
                livePreview.desiredPreviewHeight,
                livePreview.minimumPreviewHeight)

            width = height * resourcePreview.effectiveAspectRatio
            return Qt.size(width, height)
        }

        width: Math.min(desiredSize.width, livePreview.maximumPreviewWidth)
        height: Math.min(desiredSize.height, livePreview.maximumPreviewHeight)

        parent: livePreview.contentItem
        y: -height
        visible: !!resourcePreview.source

        color: "black"

        Rectangle
        {
            anchors.fill: parent
            color: ColorTheme.transparent(livePreview.baseColor, 0.3)
            clip: true

            ResourcePreview
            {
                id: resourcePreview

                width: previewHolder.desiredSize.width
                height: previewHolder.desiredSize.height
                anchors.centerIn: parent

                loadingIndicatorTimeoutMs: 0 //< Never times out.
                blurImageWhileUpdating: true

                SecurityOverlay
                {
                    id: securityOverlay

                    parent: resourcePreview.imageArea
                    anchors.fill: parent
                    resource: (resourcePreview.source && resourcePreview.source.resource) || null
                    mode: SecurityOverlay.Archive
                    font.pixelSize: 18
                }
            }
        }
    }

    Rectangle
    {
        id: markerLine

        readonly property real offset: livePreview.pointerLength - 2

        parent: livePreview.contentItem
        x: livePreview.pointerPos.x - 1
        y: (parent ? parent.height : 0) + offset
        height: Math.max(0, livePreview.markerLineLength - offset)
        width: 2
        color: livePreview.baseColor
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtMultimedia

import Nx.Core 1.0

import nx.vms.client.core 1.0

MultiVideoLayout
{
    id: layout

    readonly property size channelSize: d.sourceSize
    property MediaPlayer mediaPlayer: null
    property Component channelOverlay: null

    // Implementation.

    implicitWidth: d.sourceSize.width * layoutSize.width
    implicitHeight: d.sourceSize.height * layoutSize.height

    function pointInVideo(position)
    {
        for (var i = 0; i != layout.repeater.count; ++i)
        {
            var child = layout.repeater.itemAt(i);
            var mapped = mapToItem(child, position.x, position.y)
            if (mapped.x < 0 || mapped.y < 0 || mapped.x > child.width || mapped.y > child.height)
                continue;

            return true
        }
        return false
    }

    function getMoveViewportData(position)
    {
        for (var i = 0; i != repeater.count; ++i)
        {
            var child = repeater.itemAt(i);
            if (typeof child.getMoveViewportData !== "function")
                continue

            var mapped = mapToItem(child, position.x, position.y)
            if (mapped.x < 0 || mapped.y < 0 || mapped.x > child.width || mapped.y > child.height)
                continue

            return child.getMoveViewportData(mapped)
        }
        return null
    }

    delegate: VideoOutput
    {
        id: videoOutput
        fillMode: VideoOutput.Stretch

        readonly property int index: model.index

        MediaPlayerChannelAdapter
        {
            mediaPlayer: layout.mediaPlayer
            channel: index
            videoSurface: videoOutput.videoSink
        }

        onSourceRectChanged:
        {
            if (index === 0 && d.sourceSize.width <= 0.0)
                d.sourceSize = Qt.size(sourceRect.width, sourceRect.height)
        }

        Loader
        {
            anchors.fill: parent
            sourceComponent: channelOverlay

            onLoaded:
            {
                if (item.hasOwnProperty("index"))
                    item.index = index
            }
        }

        function getMoveViewportData(position)
        {
            var source = videoOutput.sourceRect
            var scale =  source.width / width
            var pos = Qt.vector2d(position.x, position.y).times(scale)
            if (pos.x < 0 || pos.y < 0 || pos.x > source.width || pos.y >source.height)
                return

            var center = Qt.vector2d(source.width / 2, source.height / 2)
            var topLeft = pos.minus(center)
            var newViewport = Qt.rect(
                topLeft.x / source.width,
                topLeft.y / source.height,
                1, 1)

            var videoAspect = videoOutput.sourceRect.width / videoOutput.sourceRect.height
            var result = {"channeId": index, "viewport": newViewport, "aspect": videoAspect}
            return result
        }
    }

    NxObject
    {
        id: d

        property size sourceSize
    }
}

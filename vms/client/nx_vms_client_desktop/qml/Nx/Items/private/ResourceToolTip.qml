// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Core.Items
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

BubbleToolTip
{
    id: toolTip

    property alias thumbnailSource: preview.source

    /** Minimum and maximum width or height of preview image. */
    property int maximumThumbnailSize: 400
    property int minimumThumbnailSize: 56

    /** A delay after which a live video stream is opened instead of static preview. */
    property int videoDelayMs: 3000
    readonly property bool videoPreviewEnabled: videoDelayMs > 0
    property alias videoQuality: videoPreview.videoQuality

    readonly property alias resource: resourceHelper.resource

    color: ColorTheme.colors.resourceTree.tooltip.background

    ResourceHelper
    {
        id: resourceHelper
        resource: (thumbnailSource && thumbnailSource.resource) || null
    }

    contentItem: Item
    {
        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        Column
        {
            id: column

            anchors.fill: parent
            spacing: 8

            Item
            {
                id: thumbnailPreview

                visible: resourceHelper.hasVideo

                implicitWidth: Math.max(minimumThumbnailSize, Math.min(
                    maximumThumbnailSize,
                    preview.implicitWidth,
                    maximumThumbnailSize * preview.effectiveAspectRatio))

                implicitHeight: Math.max(minimumThumbnailSize, Math.min(
                    maximumThumbnailSize,
                    preview.implicitHeight,
                    maximumThumbnailSize / preview.effectiveAspectRatio))

                Rectangle
                {
                    id: previewBackground

                    anchors.fill: parent
                    color: ColorTheme.colors.resourceTree.tooltip.previewBackground
                    border.color: ColorTheme.colors.resourceTree.tooltip.previewFrame
                    radius: 1
                }

                PreserveAspectFitter
                {
                    mode: PreserveAspectFitter.Crop
                    anchors.fill: parent

                    contentItem: ResourcePreview
                    {
                        id: preview

                        noDataText: qsTr("Cannot load preview")
                        obsoletePreviewText: qsTr("Preview is outdated")
                        markObsolete: !videoPreview.visible
                        loadingIndicatorDotSize: 10
                        textSize: 20
                        fontWeight: Font.Thin

                        foregroundColor: ColorTheme.colors.resourceTree.tooltip.foreground
                        obsolescenceDimmerColor:
                            ColorTheme.colors.resourceTree.tooltip.obsolescenceDimmer
                        obsolescenceTextColor:
                            ColorTheme.colors.resourceTree.tooltip.obsolescenceText

                        SecurityOverlay
                        {
                            id: securityOverlay

                            parent: preview.imageArea
                            anchors.fill: parent
                            resource: resourceHelper.resource
                        }
                    }
                }

                CameraDisplay
                {
                    id: videoPreview

                    parent: preview.imageArea
                    anchors.fill: parent
                    videoQuality: MediaPlayer.LowIframesOnlyVideoQuality
                    audioEnabled: false
                    visible: loaded && !securityOverlay.active
                    forcedAspectRatio: preview.sourceAspectRatio

                    tag: "ResourceTooltip"

                    function saveVideoFrame()
                    {
                        if (!mediaPlayer.playing || !preview.source.setFallbackImage)
                            return

                        const timestampMs = videoPreview.mediaPlayer.position
                        const rotationQuadrants = preview.source.rotationQuadrants

                        ItemGrabber.grabToImage(videoPreview,
                            function (resultUrl)
                            {
                                preview.source.setFallbackImage(resultUrl, timestampMs,
                                    videoPreview.videoRotation / 90)
                            })
                    }

                    Timer
                    {
                        running: videoPreview.loaded && videoPreview.mediaPlayer.playing
                        triggeredOnStart: true
                        interval: 1000
                        repeat: true

                        onTriggered:
                            videoPreview.saveVideoFrame()
                    }
                }

                Timer
                {
                    id: videoTimer

                    interval: toolTip.videoDelayMs
                    repeat: false

                    onTriggered:
                    {
                        if (!toolTip.visible || !resourceHelper.isVideoCamera)
                            return

                        videoPreview.cameraResourceId = toolTip.resource.id
                        videoPreview.mediaPlayer.playLive()
                    }
                }
            }

            Text
            {
                id: label

                text: toolTip.text
                font: toolTip.font
                color: toolTip.textColor
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.RichText
                visible: !resourceHelper.hasVideo
            }
        }
    }

    onVisibleChanged:
        updateVideoPreview()

    onResourceChanged:
        updateVideoPreview()

    onVideoPreviewEnabledChanged:
        updateVideoPreview()

    function updateVideoPreview()
    {
        videoPreview.mediaPlayer.stop()
        videoPreview.cameraResourceId = NxGlobals.uuid("")
        videoTimer.stop()

        if (visible && videoDelayMs > 0 && resourceHelper.isVideoCamera)
            videoTimer.start()
    }
}

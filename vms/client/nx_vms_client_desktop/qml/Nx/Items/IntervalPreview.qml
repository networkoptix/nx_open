// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: preview

    property int delayMs: LocalSettings.iniConfigValue("intervalPreviewDelayMs")
    property int loopDelayMs: LocalSettings.iniConfigValue("intervalPreviewLoopDelayMs")
    property bool active: false
    property var resourceId: NxGlobals.uuid("")

    property real timestampMs: -1
    property real durationMs: LocalSettings.iniConfigValue("intervalPreviewDurationMs")
    property real startTimeMs: timestampMs - durationMs / 2

    property real speedFactor: LocalSettings.iniConfigValue("intervalPreviewSpeedFactor")

    property real aspectRatio: 1.0

    readonly property alias previewState: loader.previewState

    property bool audioEnabled: false
    property bool autoRepeat: true

    readonly property bool playing: loader.item ? loader.item.mediaPlayer.playing : false
    readonly property var position: loader.item ? loader.item.mediaPlayer.position : 0
    readonly property bool atEnd:
    {
        return loader.item
            ? loader.item.mediaPlayer.mediaStatus == MediaPlayer.EndOfMedia
            : false
    }

    property bool forcePaused: false //< when pause() is called/pressed by the user.

    readonly property bool audioSupported: loader.item && loader.item.audioSupported

    property var color: ColorTheme.window

    readonly property bool isReady: loader.previewState === RightPanel.PreviewState.ready

    function play(position)
    {
        forcePaused = false
        loader.startPlaying(position)
    }

    function pause()
    {
        forcePaused = true
        loader.pausePlaying()
    }

    function setPosition(position)
    {
        if (!loader.item)
            return

        loader.item.mediaPlayer.position = position
    }

    Loader
    {
        id: loader

        active: false
        anchors.fill: preview

        property int previewState: { return RightPanel.PreviewState.initial }

        sourceComponent: Component
        {
            CameraDisplay
            {
                id: cameraDisplay

                cameraResourceId: preview.resourceId
                forcedAspectRatio: preview.aspectRatio
                videoQuality: MediaPlayer.LowVideoQuality
                audioEnabled: preview.audioEnabled
                visible: preview.isReady
                color: preview.color

                mediaPlayer.autoJumpPolicy: MediaPlayer.DisableJumpToLive
                mediaPlayer.speed: preview.speedFactor

                videoOverlayComponent: SecurityOverlay
                {
                    resourceId: preview.resourceId
                    mode: SecurityOverlay.Archive
                }

                Connections
                {
                    target: cameraDisplay.mediaPlayer

                    function onMediaStatusChanged()
                    {
                        switch (cameraDisplay.mediaPlayer.mediaStatus)
                        {
                            case MediaPlayer.Loaded:
                                loader.previewState = RightPanel.PreviewState.ready
                                break

                            case MediaPlayer.Loading:
                                if (loader.previewState === RightPanel.PreviewState.initial)
                                    loader.previewState = RightPanel.PreviewState.busy
                                break

                            case MediaPlayer.EndOfMedia:
                                if (loader.previewState !== RightPanel.PreviewState.loading)
                                {
                                    if (preview.autoRepeat && !preview.forcePaused)
                                        restartTimer.start()
                                }
                                else
                                {
                                    loader.previewState = RightPanel.PreviewState.missing
                                }
                                break

                            case MediaPlayer.NoVideoStreams:
                            case MediaPlayer.NoMedia:
                            case MediaPlayer.InvalidMedia:
                                loader.previewState = RightPanel.PreviewState.missing
                                break
                        }
                    }
                }
            }
        }

        function startPlaying(position)
        {
            if (!item)
                return

            item.mediaPlayer.setPlaybackMask(preview.startTimeMs, preview.durationMs)
            item.mediaPlayer.position = typeof position === "undefined" ? preview.startTimeMs : position
            item.mediaPlayer.play()
        }

        function pausePlaying()
        {
            if (!item)
                return

            item.mediaPlayer.pause()
        }

        onLoaded:
            startPlaying()
    }

    Timer
    {
        id: startTimer

        interval: preview.delayMs
        repeat: false

        readonly property bool effectivelyActive: preview.visible
            && preview.active
            && !preview.resourceId.isNull()
            && preview.startTimeMs >= 0
            && preview.durationMs > 0

        onEffectivelyActiveChanged:
        {
            if (effectivelyActive)
            {
                start()
            }
            else
            {
                stop()
                loader.active = false
                loader.previewState = RightPanel.PreviewState.initial
            }
        }

        onTriggered:
            loader.active = true
    }

    Timer
    {
        id: restartTimer

        interval: preview.loopDelayMs
        repeat: false

        onTriggered:
            loader.startPlaying()
    }

    onResourceIdChanged:
    {
        loader.previewState = RightPanel.PreviewState.initial
        preview.play()
    }

    onTimestampMsChanged:
        loader.previewState = RightPanel.PreviewState.initial
}

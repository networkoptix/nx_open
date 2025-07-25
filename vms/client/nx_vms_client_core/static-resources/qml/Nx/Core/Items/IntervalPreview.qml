// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Common
import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Items

import nx.vms.client.core

Item
{
    id: control

    property int delayMs: CoreSettings.iniConfigValue("intervalPreviewDelayMs")
    property int loopDelayMs: 0
    property bool active: false
    property Resource resource: null
    property bool autoplay: true
    property bool snippedPreview: false

    property real durationMs: 0
    property real startTimeMs: -1

    property real speedFactor: 1.0

    property real aspectRatio: 1.0

    readonly property alias previewState: loader.previewState
    readonly property bool cannotDecryptMedia:
        player?.error === MediaPlayer.CannotDecryptMedia ?? false

    property bool audioEnabled: false
    property bool autoRepeat: true
    property bool hasPreloader: false

    readonly property real videoAspectRatio: loader.item
        ? loader.item.content.mediaPlayer.aspectRatio
        : 16 / 9
    readonly property real videoRotation: loader.item
        ? loader.item.content.defaultRotation
        : 0
    readonly property MediaPlayer player: loader.item ? loader.item.content.mediaPlayer : null
    readonly property bool playing: loader.item ? loader.item.content.mediaPlayer.playing : false
    readonly property bool playingPlayerState: loader.item
        ? loader.item.content.mediaPlayer.playbackState == MediaPlayer.Playing
        : false
    readonly property var position: loader.item ? loader.item.content.mediaPlayer.position : 0
    readonly property bool atEnd:
    {
        return loader.item
            ? loader.item.content.mediaPlayer.mediaStatus == MediaPlayer.EndOfMedia
            : false
    }

    property bool scalable: false
    property bool forcePaused: false //< when pause() is called/pressed by the user.

    readonly property bool audioSupported: loader.item && loader.item.content.audioSupported

    property var color: ColorTheme.colors.dark5

    readonly property bool isReady: loader.previewState === EventSearch.PreviewState.ready

    signal clicked
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

    function stop()
    {
        forcePaused = true
        if (loader.item)
            loader.item.content.mediaPlayer.stop()
    }

    function preview()
    {
        forcePaused = true
        if (loader.item)
            loader.item.content.mediaPlayer.preview()
    }

    function setPosition(position)
    {
        if (loader.item)
            loader.item.content.mediaPlayer.position = position
    }

    Loader
    {
        id: loader

        active: false
        anchors.fill: control
        visible: !cannotDecryptMedia

        property int previewState: EventSearch.PreviewState.initial

        sourceComponent: Component
        {
            ScalableContentHolder
            {
                clip: true
                interactive: control.scalable
                minContentWidth: control.width
                minContentHeight: control.height
                maxContentWidth: control.width * maxZoomFactor
                maxContentHeight: control.height * maxZoomFactor
                doubleTapStartCheckFuncion: function() { return true }

                onClicked: control.clicked()

                CameraDisplay
                {
                    id: cameraDisplay

                    width: contentWidth
                    height: contentHeight
                    cameraResource: control.resource
                    forcedAspectRatio: control.aspectRatio
                    videoQuality: MediaPlayer.LowVideoQuality
                    audioEnabled: control.audioEnabled
                    visible: control.isReady
                    color: control.color

                    mediaPlayer.autoJumpPolicy: MediaPlayer.DisableJumpToLive
                    mediaPlayer.speed: control.speedFactor

                    Watermark
                    {
                        parent: cameraDisplay.videoOutput
                        anchors.fill: parent

                        resource: control.resource
                        sourceSize: cameraDisplay.videoOutput.channelSize
                    }

                    Connections
                    {
                        target: loader.item ? loader.item.content.mediaPlayer : null

                        function onMediaStatusChanged()
                        {
                            switch (loader.item.content.mediaPlayer.mediaStatus)
                            {
                                case MediaPlayer.Loaded:
                                    loader.previewState = EventSearch.PreviewState.ready
                                    break

                                case MediaPlayer.Loading:
                                    if (loader.previewState === EventSearch.PreviewState.initial)
                                        loader.previewState = EventSearch.PreviewState.busy
                                    break

                                case MediaPlayer.EndOfMedia:
                                    if (loader.previewState !== EventSearch.PreviewState.loading)
                                    {
                                        if (control.autoRepeat && !control.forcePaused)
                                            restartTimer.start()
                                    }
                                    else
                                    {
                                        loader.previewState = EventSearch.PreviewState.missing
                                    }
                                    break

                                case MediaPlayer.NoVideoStreams:
                                case MediaPlayer.NoMedia:
                                case MediaPlayer.InvalidMedia:
                                    loader.previewState = EventSearch.PreviewState.missing
                                    break
                            }
                        }
                    }
                }
            }
        }

        function startPlaying(position)
        {
            if (!item || control.durationMs === 0)
                return

            if (control.snippedPreview && control.durationMs > 10000)
            {
                const chunkSizeMs = 5000

                // If the video is 10 seconds or shorter, all video is played as a preview.
                // If the video is 15 seconds or shorter, the first 5 seconds and last 5
                // seconds are played. Else, the first 5 seconds must be played, then 5 seconds
                // after the exact middle of the video and last 5 seconds.
                let periods =
                    [{startTimeMs: Math.floor(control.startTimeMs), durationMs: chunkSizeMs}]
                if (control.durationMs > 15000)
                {
                    periods.push({
                        startTimeMs:
                        Math.floor(control.startTimeMs + (control.durationMs - chunkSizeMs) / 2),
                        durationMs: chunkSizeMs})
                }
                periods.push({
                    startTimeMs: Math.floor(control.startTimeMs + control.durationMs - chunkSizeMs),
                    durationMs: chunkSizeMs})
                loader.item.content.mediaPlayer.setPlaybackMask(periods)
            }
            else
            {
                loader.item.content.mediaPlayer.setPlaybackMask(
                    control.startTimeMs, control.durationMs)
            }

            loader.item.content.mediaPlayer.position = typeof position === "undefined"
                ? control.startTimeMs
                : position
            loader.item.content.mediaPlayer.play()
        }

        function pausePlaying()
        {
            if (!item)
                return

            loader.item.content.mediaPlayer.preview()
        }

        onLoaded: control.play()
    }

    NxDotPreloader
    {
        color: ColorTheme.colors.brand_contrast
        anchors.centerIn: parent
        visible:
        {
            if (!hasPreloader || cannotDecryptMedia)
                return false

           if (loader.item && loader.item.content.mediaPlayer.loading)
               return true;

            return !control.isReady
        }
    }

    Timer
    {
        id: startTimer

        interval: control.delayMs
        repeat: false

        readonly property bool effectivelyActive: control.visible
            && control.active
            && control.resource
            && control.startTimeMs >= 0
            && control.durationMs > 0

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
                loader.previewState = EventSearch.PreviewState.initial
            }
        }

        onTriggered:
            loader.active = true
    }

    Timer
    {
        id: restartTimer

        interval: control.loopDelayMs
        repeat: false

        onTriggered:
            loader.startPlaying()
    }

    onResourceChanged:
    {
        loader.previewState = EventSearch.PreviewState.initial
        if (autoplay)
            control.play()
    }

    onStartTimeMsChanged:
        loader.previewState = EventSearch.PreviewState.initial
}

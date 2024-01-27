// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0
import Nx.Items 1.0
import Nx.Core.Items 1.0
import Nx.Core.Controls 1.0

import nx.vms.client.core 1.0

Item
{
    id: control

    property int delayMs: 0
    property int loopDelayMs: 0
    property bool active: false
    property var resourceId: NxGlobals.uuid("")
    property bool autoplay: true

    property real timestampMs: -1
    property real durationMs: 1000
    property real startTimeMs: timestampMs - durationMs / 2

    property real speedFactor: 1.0

    property real aspectRatio: 1.0

    readonly property alias previewState: loader.previewState

    property bool audioEnabled: false
    property bool autoRepeat: true
    property bool hasPreloader: false

    readonly property real videoAspectRatio: loader.item
        ? loader.item.content.mediaPlayer.aspectRatio
        : 16 / 9
    readonly property real videoRotation: loader.item
        ? loader.item.content.defaultRotation
        : 0
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

    property var color: ColorTheme.colors.windowBackground

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

        property int previewState: { return EventSearch.PreviewState.initial }

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
                    width: contentWidth
                    height: contentHeight
                    cameraResourceId: control.resourceId
                    forcedAspectRatio: control.aspectRatio
                    videoQuality: MediaPlayer.LowVideoQuality
                    audioEnabled: control.audioEnabled
                    visible: control.isReady
                    color: control.color

                    mediaPlayer.autoJumpPolicy: MediaPlayer.DisableJumpToLive
                    mediaPlayer.speed: control.speedFactor

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
            if (!item)
                return

            loader.item.content.mediaPlayer.setPlaybackMask(control.startTimeMs, control.durationMs)
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

    ThreeDotBusyIndicator
    {
        color: ColorTheme.colors.brand_contrast
        anchors.centerIn: parent
        visible:
        {
            if (!hasPreloader)
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
            && !control.resourceId.isNull()
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

    onResourceIdChanged:
    {
        loader.previewState = EventSearch.PreviewState.initial
        if (autoplay)
            control.play()
    }

    onTimestampMsChanged:
        loader.previewState = EventSearch.PreviewState.initial
}

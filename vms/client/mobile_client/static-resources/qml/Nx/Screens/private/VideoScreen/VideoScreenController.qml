// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Ui
import Nx.Items
import Nx.Mobile

import nx.client.mobile
import nx.vms.api
import nx.vms.client.core
import nx.vms.client.mobile

NxObject
{
    id: controller

    readonly property alias audioController: audioController
    readonly property alias resource: resourceHelper.resource

    readonly property bool serverOffline: sessionManager.hasReconnectingSession
    readonly property bool cameraOffline:
        mediaPlayer.liveMode
            && resourceHelper.resourceStatus === API.ResourceStatus.offline
    readonly property bool cameraUnauthorized:
        mediaPlayer.liveMode
            && resourceHelper.resourceStatus === API.ResourceStatus.unauthorized
    readonly property bool noVideoStreams: mediaPlayer.noVideoStreams
    readonly property bool failed: mediaPlayer.failed
    readonly property bool offline: serverOffline || cameraOffline
    readonly property bool noLicenses: resourceHelper.analogCameraWithoutLicense;
    readonly property bool hasDefaultCameraPassword: resourceHelper.hasDefaultCameraPassword
    readonly property bool hasOldFirmware: resourceHelper.hasOldCameraFirmware
    readonly property bool tooManyConnections: mediaPlayer.tooManyConnectionsError
    readonly property bool cannotDecryptMedia: mediaPlayer.cannotDecryptMediaError
    readonly property bool noVideo: noVideoStreams || !resourceHelper.hasVideo
    readonly property bool ioModuleWarning:
         noVideo && resourceHelper.isIoModule && !resourceHelper.audioSupported
    readonly property bool ioModuleAudioPlaying:
        noVideo && resourceHelper.isIoModule && resourceHelper.audioSupported
    readonly property bool liveVirtualCamera: resourceHelper.isVirtualCamera && mediaPlayer.liveMode
    readonly property bool audioOnlyMode: mediaPlayer.audioOnlyMode

    readonly property string dummyState:
    {
        if (serverOffline)
            return "serverOffline"
        if (hasDefaultCameraPassword)
            return "defaultPasswordAlert"
        if (hasOldFirmware)
            return "oldFirmwareAlert"
        if (cameraUnauthorized)
            return "cameraUnauthorized"
        if (cameraOffline)
            return "cameraOffline"
        if (noVideoStreams)
            return "noVideoStreams"
        if (audioOnlyMode)
            return "audioOnlyMode"
        if (ioModuleWarning)
            return "ioModuleWarning"
        if (ioModuleAudioPlaying)
            return "ioModuleAudioPlaying"
        if (liveVirtualCamera)
            return "noLiveStream"
        if (cannotDecryptMedia)
            return "cannotDecryptMedia"
        if (failed)
            return "videoLoadingFailed"
        if (noLicenses)
            return "noLicenses";
        if (tooManyConnections)
            return "tooManyConnections"
        return ""
    }

    property alias resourceHelper: resourceHelper
    property alias accessRightsHelper: accessRightsHelper
    property alias mediaPlayer: mediaPlayer
    property VideoNavigation navigator

    NxObject
    {
        id: timelineController

        readonly property bool mediaLoaded: mediaPlayer.mediaStatus === MediaPlayer.Loaded
        readonly property bool loadingChunks: navigator.timeline.chunkProvider.loading
            || navigator.timeline.chunkProvider.loadingMotion

        onLoadingChunksChanged:
        {
            if (loadingChunks || !d.tryFixVirtualCameraPosition)
                return

            d.tryFixVirtualCameraPosition = false

            if (controller.liveVirtualCamera)
                jumpToFirstChunk();
        }

        ChunkPositionWatcher
        {
            id: chunkPositionWatcher

            motionSearchMode: navigator.motionSearchMode
            position: mediaPlayer.position
            chunkProvider: navigator.timeline.chunkProvider
        }

        onMediaLoadedChanged:
        {
            timelineController.updateTimelinePosition()
        }

        Connections
        {
            target: mediaPlayer

            function onPositionChanged()
            {
                timelineController.updateTimelinePosition()
            }
        }

        function setTimelinePosition(position, immediate)
        {
            if (immediate)
                navigator.timeline.timelineView.setPositionImmediately(position)
            else
                navigator.timeline.position = position
        }

        function updateTimelinePosition()
        {
            if (mediaPlayer.mediaStatus !== MediaPlayer.Loaded)
                return

            var liveMode = mediaPlayer.liveMode && !navigator.playback.paused
            if (!liveMode && !navigator.timeline.moving)
            {
                navigator.timeline.autoReturnToBounds = false
                navigator.timeline.position = mediaPlayer.position
            }
        }

        function timelineJump(position)
        {
            navigator.timeline.autoReturnToBounds = false
            navigator.timeline.timelineView.setPositionImmediately(position)
        }
    }

    NxObject
    {
        id: d

        property bool tryFixVirtualCameraPosition: false
        property bool playing: false

        property real lastPosition: -1
        property bool waitForLastPosition: false
        property bool waitForFirstPosition: true

        function savePosition()
        {
            lastPosition = currentPosition()
            waitForLastPosition = true
        }

        function currentPosition()
        {
            if (mediaPlayer.liveMode)
                return d.playing ? -1 : (new Date()).getTime()

            return mediaPlayer.position;
        }
    }

    MediaPlaybackInterruptor
    {
        id: playbackInterruptor

        player: mediaPlayer
        playable: sessionManager.hasConnectedSession
            && !navigator.forceVideoPause

        interruptOnInactivity: CoreUtils.isMobile()
    }

    MediaResourceHelper
    {
        id: resourceHelper
    }

    QnCameraAccessRightsHelper
    {
        id: accessRightsHelper
        resource: resourceHelper.resource
    }

    AudioController
    {
        id: audioController

        resource: controller.resource
        serverSessionManager: sessionManager
    }

    MediaPlayer
    {
        id: mediaPlayer

        audioEnabled:
        {
            if (!audioController.audioEnabled)
                return false

            if (videoScreen.activePage)
                return true

            return uiController.currentScreen === Controller.CameraSettingsScreen
                && stackView.currentItem.audioController === audioController
        }

        resource: resourceHelper.resource
        onPlayingChanged: setKeepScreenOn(playing)
        maxTextureSize: getMaxTextureSize()
        allowHardwareAcceleration: settings.enableHardwareDecoding
        onPositionChanged:
        {
            if (d.waitForLastPosition)
            {
                d.lastPosition = d.currentPosition()
                d.waitForLastPosition = false
            }

            if (d.waitForFirstPosition)
            {
                timelineController.timelineJump(mediaPlayer.position)
                d.waitForFirstPosition = false
            }
        }
    }

    NetworkInterfaceWatcher
    {
        onInterfacesChanged: mediaPlayer.invalidateServer()
    }

    Timer
    {
        id: tryPlayTimer

        interval: 0
        onTriggered:
        {
            // We try to play even if camera is offline.
            if (!cameraUnauthorized && d.playing && !mediaPlayer.playing)
                mediaPlayer.play()
        }
    }

    onFailedChanged:
    {
        if (failed)
            mediaPlayer.stop()
    }

    onCameraUnauthorizedChanged: tryPlayTimer.restart()

    onResourceChanged:
    {
        tryPlayTimer.restart();

        timelineController.timelineJump(d.lastPosition)
        mediaPlayer.position = d.lastPosition

        if (mediaPlayer.position == -1)
        {
            d.waitForFirstPosition = false
        }
        else if (resourceHelper.resourceStatus !== API.ResourceStatus.online)
        {
            d.waitForFirstPosition = false
            timelineController.timelineJump(mediaPlayer.position)
        }
        else
        {
            d.waitForFirstPosition = true
        }
    }

    Component.onDestruction: setKeepScreenOn(false)

    function start(targetResource, timestamp)
    {
        setResource(targetResource)

        mediaPlayer.maxTextureSize = getMaxTextureSize()
        if (timestamp && timestamp > 0)
        {
            if (Qt.application.state !== Qt.ApplicationActive)
                playbackInterruptor.setInterruptedPosition(timestamp)
            setPosition(timestamp, true)
            timelineController.setTimelinePosition(timestamp, true)
            play()
        }
        else
        {
            playLive()
        }
    }

    function play()
    {
        d.playing = true
        mediaPlayer.play()
        d.savePosition()
    }

    function playLive()
    {
        d.playing = true
        mediaPlayer.playLive()
        d.savePosition()
    }

    function stop()
    {
        d.playing = false
        mediaPlayer.stop()
    }

    function pause()
    {
        d.playing = false
        mediaPlayer.pause()
        d.savePosition();
    }

    function preview()
    {
        mediaPlayer.preview()
    }

    function setPosition(position, save)
    {
        mediaPlayer.position = position
        if (save)
            d.savePosition()
    }

    // Forces timeline to visual jump to the specified position.
    function forcePosition(position, save, immediate)
    {
        timelineController.setTimelinePosition(position, immediate)
        setPosition(position, save)
    }

    function jumpForward()
    {
        var nextChunkStartTime = chunkPositionWatcher.nextChunkStartTimeMs();
        forcePosition(nextChunkStartTime)
        if (nextChunkStartTime == -1)
            play()
    }

    function jumpBackward()
    {
        forcePosition(chunkPositionWatcher.prevChunkStartTimeMs())
    }

    function jumpToFirstChunk()
    {
        forcePosition(chunkPositionWatcher.firstChunkStartTimeMs())
    }

    function setResource(resource)
    {
        d.tryFixVirtualCameraPosition = true
        resourceHelper.resource = resource
    }

    onServerOfflineChanged:
    {
        if (serverOffline)
            return;

        if (d.playing && !(mediaPlayer.playing || mediaPlayer.loading))
            mediaPlayer.play()
    }
}

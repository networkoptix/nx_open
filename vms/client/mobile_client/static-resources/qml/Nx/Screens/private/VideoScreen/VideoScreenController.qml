import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Media 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0
import nx.client.mobile 1.0

Object
{
    id: controller

    readonly property alias resourceId: resourceHelper.resourceId

    readonly property bool serverOffline: ConnectionController.reconnecting
    readonly property bool cameraOffline:
        mediaPlayer.liveMode
            && resourceHelper.resourceStatus === MediaResourceHelper.Offline
    readonly property bool cameraUnauthorized:
        mediaPlayer.liveMode
            && resourceHelper.resourceStatus === MediaResourceHelper.Unauthorized
    readonly property bool noVideoStreams: mediaPlayer.noVideoStreams
    readonly property bool failed: mediaPlayer.failed
    readonly property bool offline: serverOffline || cameraOffline
    readonly property bool noLicenses: resourceHelper.analogCameraWithoutLicense;
    readonly property bool hasDefaultCameraPassword: resourceHelper.hasDefaultCameraPassword
    readonly property bool hasOldFirmware: resourceHelper.hasOldCameraFirmware
    readonly property bool tooManyConnections: mediaPlayer.tooManyConnectionsError
    readonly property bool noVideo: noVideoStreams || !resourceHelper.hasVideo
    readonly property bool ioModuleWarning:
         noVideo && resourceHelper.isIoModule && !resourceHelper.audioSupported
    readonly property bool ioModuleAudioPlaying:
        noVideo && resourceHelper.isIoModule && resourceHelper.audioSupported
    readonly property bool liveWearableCamera: resourceHelper.isWearableCamera && mediaPlayer.liveMode

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
        if (ioModuleWarning)
            return "ioModuleWarning"
        if (ioModuleAudioPlaying)
            return "ioModuleAudioPlaying"
        if (liveWearableCamera)
            return "noLiveStream"
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

    Object
    {
        id: timelineController

        readonly property bool mediaLoaded: mediaPlayer.mediaStatus === MediaPlayer.Loaded
        readonly property bool loadingChunks: navigator.timeline.chunkProvider.loading
            || navigator.timeline.chunkProvider.loadingMotion

        onLoadingChunksChanged:
        {
            if (loadingChunks || !d.tryFixWearableCameraPosition)
                return

            d.tryFixWearableCameraPosition = false

            if (controller.liveWearableCamera)
                jumpToFirstChunk();
        }

        ChunkPositionWatcher
        {
            id: chunkPositionWatcher

            motionSearchMode: navigator.motionSearchMode
            position: mediaPlayer.position
            chunkProvider: navigator.timeline.chunkProvider
        }

        onMediaLoadedChanged: timelineController.updateTimelinePosition()

        Connections
        {
            target: mediaPlayer
            onPositionChanged: timelineController.updateTimelinePosition()
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

    QtObject
    {
        id: d

        readonly property bool applicationActive: Qt.application.state === Qt.ApplicationActive

        property bool tryFixWearableCameraPosition: false
        property bool playing: false

        property real lastPosition: -1
        property real interruptedPosition: -1
        property bool waitForLastPosition: false
        property bool waitForFirstPosition: true

        function savePosition()
        {
            lastPosition = currentPosition()
            waitForLastPosition = true
        }

        function interrupt()
        {
            d.interruptedPosition = d.currentPosition()
            mediaPlayer.stop()
        }

        function resumePlaying()
        {
            mediaPlayer.position = interruptedPosition
            mediaPlayer.play()
        }

        onApplicationActiveChanged:
        {
            if (!Utils.isMobile())
                return

            if (!applicationActive)
                interrupt()
            else if (playing)
                resumePlaying()
        }

        function currentPosition()
        {
            if (mediaPlayer.liveMode)
                return d.playing ? -1 : (new Date()).getTime()

            return mediaPlayer.position;
        }
    }

    MediaResourceHelper
    {
        id: resourceHelper
    }

    QnCameraAccessRightsHelper
    {
        id: accessRightsHelper
        resourceId: resourceHelper.resourceId
    }

    MediaPlayer
    {
        id: mediaPlayer
        audioEnabled: audioController.audioEnabled

        resourceId: resourceHelper.resourceId
        onPlayingChanged: setKeepScreenOn(playing)
        maxTextureSize: getMaxTextureSize()
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

    onResourceIdChanged:
    {
        tryPlayTimer.restart();

        timelineController.timelineJump(d.lastPosition)
        mediaPlayer.position = d.lastPosition

        if (mediaPlayer.position == -1)
        {
            d.waitForFirstPosition = false
        }
        else if (resourceHelper.resourceStatus !== MediaResourceHelper.Online)
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

    function start(targetResourceId, timestamp)
    {
        setResourceId(targetResourceId)

        mediaPlayer.maxTextureSize = getMaxTextureSize()
        if (timestamp && timestamp > 0)
        {
            setPosition(timestamp, true)
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

    function setResourceId(id)
    {
        d.tryFixWearableCameraPosition = true
        resourceHelper.resourceId = id
        audioController.resourceId = id
    }
}

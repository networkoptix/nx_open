import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Media 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Object
{
    id: videoScreenController

    property alias resourceId: resourceHelper.resourceId

    readonly property bool serverOffline:
        connectionManager.connectionState === QnConnectionManager.Reconnecting
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
        if (failed)
            return "videoLoadingFailed"
        if (noLicenses)
            return "noLicenses";

        return ""
    }

    property alias resourceHelper: resourceHelper
    property alias accessRightsHelper: accessRightsHelper
    property alias mediaPlayer: mediaPlayer

    signal playerJump(real position)
    signal gotFirstPosition(real position)

    QtObject
    {
        id: d

        readonly property bool applicationActive: Qt.application.state === Qt.ApplicationActive

        property bool playing: false

        property real lastPosition: -1
        property bool waitForLastPosition: false
        property bool waitForFirstPosition: true

        function savePosition()
        {
            lastPosition = currentPosition()
            waitForLastPosition = true
        }

        onApplicationActiveChanged:
        {
            if (!Utils.isMobile())
                return

            if (!applicationActive)
                mediaPlayer.pause()
            else if (d.playing)
                mediaPlayer.play()
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
                playerJump(mediaPlayer.position)
                d.waitForFirstPosition = false
                gotFirstPosition(mediaPlayer.position)
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

        playerJump(d.lastPosition)
        mediaPlayer.position = d.lastPosition

        if (mediaPlayer.position == -1)
        {
            d.waitForFirstPosition = false
        }
        else if (resourceHelper.resourceStatus !== MediaResourceHelper.Online)
        {
            d.waitForFirstPosition = false
            gotFirstPosition(mediaPlayer.position)
        }
        else
        {
            d.waitForFirstPosition = true
        }
    }

    Component.onDestruction: setKeepScreenOn(false)

    function start()
    {
        if (cameraOffline || cameraUnauthorized || resourceId === "")
            return

        mediaPlayer.maxTextureSize = getMaxTextureSize()
        playLive()
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

    function setPosition(position, savePosition)
    {
        mediaPlayer.position = position
        if (savePosition)
            d.savePosition()
    }
}

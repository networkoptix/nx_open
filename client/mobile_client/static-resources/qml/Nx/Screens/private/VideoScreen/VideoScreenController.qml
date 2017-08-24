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

    readonly property string dummyState:
    {
        if (serverOffline)
            return "serverOffline"
        else if (cameraUnauthorized)
            return "cameraUnauthorized"
        else if (cameraOffline)
            return "cameraOffline"
        else if (noVideoStreams)
            return "noVideoStreams"
        else if (failed)
            return "videoLoadingFailed"
        else
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

        property bool resumeOnActivate: false
        property bool resumeOnOnline: false
        property real lastPosition: -1
        property bool waitForLastPosition: false
        property bool waitForFirstPosition: true

        function savePosition()
        {
            lastPosition = mediaPlayer.liveMode ? -1 : mediaPlayer.position
            waitForLastPosition = true
        }

        onApplicationActiveChanged:
        {
            if (!Utils.isMobile())
                return

            if (applicationActive)
            {
                if (d.resumeOnActivate)
                    mediaPlayer.play()
            }
            else
            {
                d.resumeOnActivate = mediaPlayer.playing
                mediaPlayer.pause()
            }
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
                d.lastPosition = position
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

    onFailedChanged:
    {
        if (failed)
        {
            d.resumeOnOnline = offline
            mediaPlayer.stop()
        }
    }

    onOfflineChanged:
    {
        if (!offline && d.resumeOnOnline)
        {
            d.resumeOnOnline = false
            mediaPlayer.play()
        }
    }

    onResourceIdChanged:
    {
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
        mediaPlayer.play()
        d.savePosition()
    }

    function playLive()
    {
        mediaPlayer.playLive()
    }

    function stop()
    {
        mediaPlayer.stop()
    }

    function pause()
    {
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

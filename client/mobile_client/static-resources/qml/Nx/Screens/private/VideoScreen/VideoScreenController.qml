import QtQuick 2.6
import Nx 1.0
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
            && resourceHelper.resourceStatus === QnMediaResourceHelper.Offline
    readonly property bool cameraUnauthorized:
        mediaPlayer.liveMode
            && resourceHelper.resourceStatus === QnMediaResourceHelper.Unauthorized
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
        else if (failed)
            return "videoLoadingFailed"
        else
            return ""
    }

    property alias resourceHelper: resourceHelper
    property alias accessRightsHelper: accessRightsHelper
    property alias mediaPlayer: mediaPlayer

    signal playerJump(real position)

    QtObject
    {
        id: d
        property bool resumeOnActivate: false
        property bool resumeOnOnline: false
        property real lastPosition: -1
        property bool waitForLastPosition: false
    }

    QnMediaResourceHelper
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
        onMediaStatusChanged:
        {
            if (d.waitForLastPosition && mediaStatus == QnPlayer.Loaded)
            {
                d.lastPosition = position
                d.waitForLastPosition = false
            }
        }
    }

    Connections
    {
        target: Qt.application
        onStateChanged:
        {
            if (!Utils.isMobile())
                return

            if (Qt.application.state === Qt.ApplicationActive)
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
    }

    Component.onCompleted:
    {
        if (cameraOffline || cameraUnauthorized || resourceId == "")
            return

        mediaPlayer.playLive()
    }
    Component.onDestruction: setKeepScreenOn(false)

    function play()
    {
        mediaPlayer.play()

        d.lastPosition = mediaPlayer.liveMode ? -1 : mediaPlayer.position
        d.waitForLastPosition = (mediaPlayer.mediaStatus !== QnPlayer.Loaded)
    }

    function playLive()
    {
        mediaPlayer.position = -1
        play()
    }

    function stop()
    {
        mediaPlayer.stop()
    }

    function pause()
    {
        mediaPlayer.pause()
    }

    function preview()
    {
        mediaPlayer.preview()
    }

    function setPosition(position)
    {
        mediaPlayer.position = position
    }
}

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
        mediaPlayer.liveMode && resourceHelper.resourceStatus == QnMediaResourceHelper.Offline
    readonly property bool cameraUnauthorized:
        mediaPlayer.liveMode && resourceHelper.resourceStatus == QnMediaResourceHelper.Unauthorized
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

    QtObject
    {
        id: d
        property bool resumeOnActivate: false
        property bool resumeAtLive: false
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
    }

    Connections
    {
        target: Qt.application
        onStateChanged:
        {
            if (!Utils.isMobile())
                return

            if (Qt.application.state == Qt.ApplicationActive)
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
            mediaPlayer.stop()
    }

    Component.onCompleted:
    {
        if (cameraOffline || cameraUnauthorized || resourceId == "")
            return

        mediaPlayer.playLive()
    }
    Component.onDestruction: setKeepScreenOn(false)
}

import QtQuick 2.0
import com.networkoptix.qml 1.0

QnPlayer
{
    id: player

    property string resourceId

    readonly property bool loading: playbackState == QnPlayer.Playing && mediaStatus == QnPlayer.Loading
    readonly property bool playing: playbackState == QnPlayer.Playing && mediaStatus == QnPlayer.Loaded
    readonly property bool failed: mediaStatus == QnPlayer.NoMedia

    source: resourceId ? "camera://media/" + resourceId : ""

    function playLive()
    {
        position = -1
        play()
    }
}

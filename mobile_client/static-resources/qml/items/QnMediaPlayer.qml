import QtQuick 2.0

import com.networkoptix.qml 1.0

QnPlayer
{
    id: player

    property string resourceId

    readonly property bool loading: playbackState == QnPlayer.Playing && mediaStatus == QnPlayer.Loading
    readonly property bool playing: playbackState == QnPlayer.Playing && mediaStatus == QnPlayer.Loaded
    readonly property bool failed: mediaStatus == QnPlayer.NoMedia

	onPlayingChanged: console.log("----> playing = " + playing)
	onLoadingChanged: console.log("----> loading = " + loading)
    onLiveModeChanged:  console.log("----> live = " + liveMode)
	onPositionChanged: console.log("----> position = " + position)

    source: "camera://media/" + resourceId

    function playLive()
    {
        seek(-1)
        play()
    }

    function seek(pos)
    {
        position = pos
    }
}

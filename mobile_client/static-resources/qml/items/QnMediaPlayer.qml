import QtQuick 2.4

import QtMultimedia 5.0
import QtQuick.Window 2.0

import com.networkoptix.qml 1.0

import ".."

QnObject {
    id: player

    property string resourceId

    readonly property bool loading: nxPlayer.playbackState == QnPlayer.Playing && nxPlayer.mediaStatus == QnPlayer.Loading
    readonly property bool playing: nxPlayer.playbackState == QnPlayer.Playing && nxPlayer.mediaStatus == QnPlayer.Loaded
    readonly property bool atLive: nxPlayer.liveMode

	onPlayingChanged: console.log("----> playing = " + playing)
	onLoadingChanged: console.log("----> loading = " + loading)

    readonly property bool failed: nxPlayer.mediaStatus == QnPlayer.NoMedia

    readonly property alias position: nxPlayer.position
    readonly property bool hasTimestamp: true

    readonly property var mediaPlayer: nxPlayer
    readonly property var chunkProvider: chunkProvider
    readonly property var resourceHelper: resourceHelper

    readonly property alias resourceName: resourceHelper.resourceName

    signal timelineCorrectionRequest(real position)
    signal timelinePositionRequest(real position)

    QtObject {
        id: d

        property bool resetUrlOnConnect: false
    }

    QnPlayer {
		id: nxPlayer
        source: "camera://media/" + player.resourceId
        onPositionChanged: timelineCorrectionRequest(nxPlayer.position)
		onLiveModeChanged: timelineCorrectionRequest(nxPlayer.position)
    }

    QnMediaResourceHelper {
        id: resourceHelper
        resourceId: player.resourceId
    }

    QnCameraChunkProvider {
        id: chunkProvider
        resourceId: player.resourceId
    }

    Timer {
        interval: 30000
        triggeredOnStart: true
        running: true
        onTriggered: chunkProvider.update()
    }

    function playLive() {
        play(-1)
    }

    function play(pos) {
		if (pos)
			seek(pos)

        if (!playing)
            nxPlayer.play()

        timelinePositionRequest(nxPlayer.position)

        nxPlayer.play()
    }

    function pause() {
        nxPlayer.pause()
    }

    function stop() {
        nxPlayer.stop()
    }

    function seek(pos) {
        nxPlayer.position = pos
    }
}

import QtQuick 2.4

import QtMultimedia 5.0
import QtQuick.Window 2.0

import com.networkoptix.qml 1.0

import ".."

QnObject {
    id: player

    property string resourceId

    readonly property bool playing: mediaPlayer.playbackState === MediaPlayer.PlayingState && mediaPlayer.position > 0
    readonly property bool atLive: d.position < 0

    readonly property alias position: d.position

    readonly property var mediaPlayer: mediaPlayer
    readonly property var chunkProvider: chunkProvider

    readonly property alias resourceName: resourceHelper.resourceName
    readonly property int videoWidth: mediaPlayer.metaData.resolution ? mediaPlayer.metaData.resolution.width : 0
    readonly property int videoHeight: mediaPlayer.metaData.resolution ? mediaPlayer.metaData.resolution.height : 0

    signal timelineCorrectionRequest(real position)

    QtObject {
        id: d

        property bool paused: false
        property real position: -1
        property real chunkEnd: -1
        property int prevPlayerPosition: 0
        property bool updateTimeline: false
        property bool dirty: true
    }

    MediaPlayer {
        id: mediaPlayer
        source: resourceHelper.mediaUrl

        onPositionChanged: updatePosition()
        onSourceChanged: console.log(source)
    }

    QnMediaResourceHelper {
        id: resourceHelper

        screenSize: Qt.size(mainWindow.width, mainWindow.height)
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

    Timer {
        interval: 5000
        running: player.playing && d.position >= 0
        onTriggered: d.updateTimeline = true
    }

    function alignedPosition(pos) {
        return pos >= 0 ? chunkProvider.closestChunkStartMs(pos, true) : -1
    }

    function playLive() {
        d.dirty = true
        play(-1)
    }

    function play(pos) {
        if (!d.dirty) {
            if (!playing)
                mediaPlayer.play()
            return
        }

        mediaPlayer.stop()

        var aligned = alignedPosition(pos)
        d.position = aligned >= 0 ? Math.max(aligned, pos) : -1
        d.chunkEnd = (d.position >= 0) ? chunkProvider.closestChunkEndMs(pos, true) : -1

        timelineCorrectionRequest(d.position)
        d.updateTimeline = false
        resourceHelper.setPosition(d.position)

        d.prevPlayerPosition = 0
        mediaPlayer.play()
        d.paused = false

        d.dirty = false
    }

    function pause() {
        mediaPlayer.pause()
        d.paused = true
    }

    function seek(pos) {
        d.dirty = true

        if (d.paused) {
            d.position = pos
            timelineCorrectionRequest(d.position)
            d.updateTimeline = false
        } else {
            play(pos)
        }
    }

    function updatePosition() {
        if (d.position < 0)
            return

        d.position += mediaPlayer.position - d.prevPlayerPosition
        d.prevPlayerPosition = mediaPlayer.position

        if (d.chunkEnd >= 0 && d.position >= d.chunkEnd) {
            seek(d.chunkEnd + 1)
            return
        }

        if (d.updateTimeline) {
            timelineCorrectionRequest(d.position)
            d.updateTimeline = false
        }
    }
}


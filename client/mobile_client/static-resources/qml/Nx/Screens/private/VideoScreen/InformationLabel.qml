import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Media 1.0
import com.networkoptix.qml 1.0

Pane
{
    id: control

    property VideoScreenController videoScreenController: null

    padding: 8
    enabled: false

    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    background: Rectangle
    {
        color: ColorTheme.transparent(ColorTheme.windowBackground, 0.6)
    }

    QtObject
    {
        id: d
        property real framerate: 0
        property real bitrate: 0
        property bool isHwAccelerated: false
        property string codec: ""
        property string quality
        property string resolution:
        {
            if (videoScreenController)
            {
                var resolution = videoScreenController.mediaPlayer.currentResolution
                if (resolution.width > 0 && resolution.height > 0)
                    return resolution.width + "x" + resolution.height
            }
            return qsTr("Unknown")
        }

        function updateStatistics()
        {
            if (!videoScreenController)
                return

            var player = videoScreenController.mediaPlayer
            var statistics = player.currentStatistics()

            if (player.playing)
            {
                framerate = statistics.framerate
                bitrate = statistics.bitrate
            }
            else
            {
                framerate = 0.0
                bitrate = 0.0
            }

            codec = statistics.codec
            isHwAccelerated = statistics.isHwAccelerated

            var quality = player.actualVideoQuality()
            if (quality === MediaPlayer.HighVideoQuality)
                d.quality = qsTr("Hi-Res")
            else if (quality === MediaPlayer.LowVideoQuality)
                d.quality = qsTr("Lo-Res")
            else if (quality === MediaPlayer.LowIframesOnlyVideoQuality)
                d.quality = qsTr("Keyfr + Lo-Res")
            else if (quality === MediaPlayer.CustomVideoQuality)
                d.quality = qsTr("Custom-Res")
            else
                d.quality = ""
        }
    }

    contentItem: Column
    {
        InformationText
        {
            text: d.resolution
        }
        InformationText
        {
            text: qsTr("%1 fps").arg(d.framerate.toFixed(2))
        }
        InformationText
        {
            text: qsTr("%1 Mbps").arg(d.bitrate.toFixed(2))
        }
        InformationText
        {
            text: d.codec +
                (d.isHwAccelerated ? qsTr(" (%1)").arg("HW") : "")
        }
        InformationText
        {
            text: d.quality
        }
        InformationText
        {
            text: videoScreenController.resourceHelper.serverName
            maximumLineCount: 3
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
    }

    Timer
    {
        id: updateTimer
        running: videoScreenController && videoScreenController.mediaPlayer.playing
        repeat: true
        interval: 1000
        onTriggered: d.updateStatistics()
        onRunningChanged: d.updateStatistics()
    }
}

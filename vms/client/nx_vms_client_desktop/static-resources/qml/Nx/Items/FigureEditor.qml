import QtQuick 2.0
import QtQuick.Controls 2.0
import Nx 1.0
import Nx.Core 1.0
import Nx.Media 1.0

import "private/FigureEditor"

Item
{
    id: editor

    property Figure figure: null

    property string figureType: "polygon"
    readonly property bool hasFigure: figure && figure.hasFigure
    property color color

    property var resourceId

    MediaResourceHelper
    {
        id: helper
        resourceId: editor.resourceId ? editor.resourceId.toString() : ""
    }

    MediaPlayer
    {
        id: player

        resourceId: helper.resourceId
        audioEnabled: false

        readonly property bool loaded: mediaStatus === MediaPlayer.MediaStatus.Loaded

        onResourceIdChanged: video.clear()
        onSourceChanged: updatePlayingState()
    }

    VideoPositioner
    {
        id: content

        anchors.fill: parent

        sourceSize: Qt.size(video.implicitWidth, video.implicitHeight)
        item: video

        customAspectRatio:
        {
            var aspectRatio = helper ? helper.customAspectRatio : 0.0
            if (aspectRatio === 0.0)
            {
                if (player && player.loaded)
                    aspectRatio = player.aspectRatio
                else if (helper)
                    aspectRatio = helper.aspectRatio
                else
                    aspectRatio = sourceSize.width / sourceSize.height
            }

            var layoutSize = helper ? helper.layoutSize : Qt.size(1, 1)
            aspectRatio *= layoutSize.width / layoutSize.height

            return aspectRatio
        }

        MultiVideoOutput
        {
            id: video
            resourceHelper: helper
            mediaPlayer: player
        }
    }

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.transparent("black", 0.5)
    }

    Loader
    {
        id: figureLoader

        anchors.fill: parent

        source:
        {
            if (figureType === "line")
                return "private/FigureEditor/Line.qml"
            else if (figureType === "box")
                return "private/FigureEditor/Box.qml"
            else if (figureType === "polygon")
                return "private/FigureEditor/Polygon.qml"
            else
                return ""
        }

        onLoaded: figure = item
    }

    Binding
    {
        target: editor
        property: "color"
        value: figure.color
        when: figure
    }

    onColorChanged:
    {
        figure.color = color
    }

    onVisibleChanged: updatePlayingState()

    function updatePlayingState()
    {
        if (visible)
            player.playLive()
        else
            player.pause()
    }

    function deserialize(json)
    {
        figure.deserialize(json)
        if (!figure.hasFigure)
            figure.startCreation()
    }

    function serialize()
    {
        return figure.serialize()
    }

    function clear()
    {
        figure.startCreation()
    }
}

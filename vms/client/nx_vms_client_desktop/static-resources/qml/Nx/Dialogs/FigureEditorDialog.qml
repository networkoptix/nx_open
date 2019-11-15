import QtQuick 2.11
import QtQuick.Window 2.11
import Nx 1.0
import Nx.Items 1.0
import Nx.Controls 1.0
import Nx.Core 1.0
import Nx.Media 1.0
import Nx.Core.Items 1.0

Window
{
    id: dialog

    modality: Qt.WindowModal

    width: 600
    height: 400

    color: ColorTheme.colors.dark3

    signal accepted()

    property alias figureType: editor.figureType
    property int maxPolygonPoints: -1

    property var resourceId

    MediaResourceHelper
    {
        id: helper
        resourceId: dialog.resourceId ? dialog.resourceId.toString() : ""
    }

    MediaPlayer
    {
        id: player

        resourceId: helper.resourceId
        audioEnabled: false

        readonly property bool loaded: mediaStatus === MediaPlayer.MediaStatus.Loaded

        onResourceIdChanged: video.videoOutput.clear()
        onSourceChanged: updatePlayingState()
    }

    Item
    {
        width: parent.width
        height: parent.height - bottomPanel.height

        MultiVideoPositioner
        {
            id: video

            anchors.fill: parent

            resourceHelper: helper
            mediaPlayer: player

            FigureEditor
            {
                id: editor
                anchors.fill: video.sourceSize.width > 0 && video.sourceSize.height > 0
                    ? video.videoOutput : parent
                rotation: video.videoRotation
            }
        }

        Banner
        {
            visible: text !== ""
            anchors.bottom: parent.bottom
            text: editor.hint
            style: editor.hintStyle
        }
    }

    DialogPanel
    {
        id: bottomPanel

        implicitWidth: leftBottomControls.implicitWidth + rightBottomControls.implicitWidth + 48

        Row
        {
            id: leftBottomControls

            height: parent.height
            x: 16
            spacing: 16

            LinearPaletteColorPicker
            {
                id: palette

                anchors.verticalCenter: parent.verticalCenter

                colors: [
                    "#e040fb",
                    "#536dfe",
                    "#64ffda",
                    "#ffff00",
                    "#ffab40",
                    "#b2ff59",
                    "#7c4dff",
                    "#ff4081"
                ]

                onColorChanged: editor.color = color
            }

            Button
            {
                anchors.verticalCenter: parent.verticalCenter
                flat: true
                text: qsTr("Reset")
                iconUrl: "qrc:/skin/text_buttons/refresh.png"
                leftPadding: 0
                rightPadding: 16
                onClicked: editor.clear()
            }
        }

        Row
        {
            id: rightBottomControls

            spacing: 8
            anchors
            {
                right: parent.right
                rightMargin: 16
                verticalCenter: parent.verticalCenter
            }

            Button
            {
                text: qsTr("OK")
                isAccentButton: true
                onClicked:
                {
                    close()
                    accepted()
                }
                width: Math.max(implicitWidth, 80)
            }

            Button
            {
                text: qsTr("Cancel")
                onClicked: close()
                width: Math.max(implicitWidth, 80)
            }
        }
    }

    onVisibleChanged: updatePlayingState()

    onScreenChanged:
    {
        // Minimum size setting does not work properly when screen is changed and new screen has
        // another DPI. New unscaled size is not sent to a window manager.
        minimumWidth = 0
        minimumHeight = 0
        minimumWidth = bottomPanel.implicitWidth
        minimumHeight = 200
    }

    function updatePlayingState()
    {
        if (visible)
            player.playLive()
        else
            player.pause()
    }

    function serializeFigure()
    {
        return editor.serialize()
    }

    function deserializeFigure(json)
    {
        editor.deserialize(json)
        if (!editor.hasFigure)
            editor.color = palette.colors[Math.floor(Math.random() * palette.colors.length)]
        palette.color = editor.color
    }
}

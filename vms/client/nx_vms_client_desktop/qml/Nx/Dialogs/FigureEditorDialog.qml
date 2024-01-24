// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx
import Nx.Items
import Nx.Controls
import Nx.Core
import Nx.Core.Items
import Nx.Instruments

import nx.vms.client.core
import nx.vms.client.desktop

Window
{
    id: dialog

    modality: Qt.ApplicationModal

    width: 600
    height: 400
    minimumWidth: 600
    minimumHeight: 400

    color: ColorTheme.colors.dark3

    signal accepted()

    property alias figureType: editor.figureType
    property alias figureSettings: editor.figureSettings
    property alias player: player

    property bool showPalette: true
    property bool showClearButton: true

    property var resourceId: NxGlobals.uuid("")

    MediaResourceHelper
    {
        id: helper
        resourceId: dialog.resourceId
    }

    TextureSizeHelper
    {
        id: textureSizeHelper
    }

    MediaPlayer
    {
        id: player

        resourceId: helper.resourceId
        audioEnabled: false

        readonly property bool loaded: mediaStatus === MediaPlayer.MediaStatus.Loaded

        onSourceChanged: updatePlayingState()
        maxTextureSize: textureSizeHelper.maxTextureSize

        tag: "FigureEditor"
    }

    CursorManager
    {
        id: cursorManager
    }

    Shortcut
    {
        sequences: [StandardKey.Cancel]
        onActivated: dialog.close()
    }

    Item
    {
        id: content

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

        DialogBanner
        {
            id: banner

            visible: text !== ""
            anchors.bottom: parent.bottom
            width: parent.width
            text: editor.hint
            style: editor.hintStyle
            opacity: hovered ? 0 : 1

            property bool hovered: false

            Behavior on opacity { NumberAnimation { duration: 100 } }

            Connections
            {
                target: editor.hoverInstrument

                function onHoverMove(hover)
                {
                    hoverCheckTimer.stop()
                    banner.hovered = banner.contains(
                        banner.mapFromItem(content, hover.position.x, hover.position.y))
                }

                function onHoverLeave()
                {
                    hoverCheckTimer.start()
                }
            }

            Timer
            {
                // This is a workaround. See FigureEditor.qml.
                // We can't rely on HoverLeave event because it could happen when a pointer is
                // still inside the area, but another item is hovered (e.g. when point grip is
                // hovered we get HoverLeave here). Also we can't check mouse position in the leave
                // event because it is not always on the item boundary. Thus the simplest solution
                // is to check the global cursor position after a small delay.
                // TODO: #4.2 #dklychkov Rewrite to HoverHandler.

                id: hoverCheckTimer

                interval: 50
                running: false

                onTriggered:
                {
                    const globalPos = cursorManager.pos()
                    const pos = banner.mapFromGlobal(globalPos.x, globalPos.y)
                    if (pos.x <= 0 || pos.x >= banner.width
                        || pos.y <= 0 || pos.y >= banner.height)
                    {
                        banner.hovered = false
                    }
                }
            }
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

                visible: dialog.showPalette
                colors: ColorTheme.colors.roi.palette

                onColorChanged: editor.color = color
            }

            TextButton
            {
                text: qsTr("Clear")

                icon.source: "image://svg/skin/text_buttons/reload_20.svg"
                anchors.verticalCenter: parent.verticalCenter
                visible: dialog.showClearButton
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
                    accepted()
                    close()
                }
                width: Math.max(implicitWidth, 80)
                enabled: editor.figureAcceptable
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
            editor.color = ColorTheme.colors.roi.palette[0]
        palette.color = editor.color
    }
}

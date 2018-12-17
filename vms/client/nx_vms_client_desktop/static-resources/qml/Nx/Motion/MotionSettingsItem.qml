import QtQuick 2.6
import QtQuick.Controls 1.2

import Nx 1.0
import Nx.Core 1.0
import Nx.Core.Items 1.0
import Nx.Common 1.0
import Nx.Media 1.0
import Nx.Items 1.0
import Nx.Motion 1.0
import com.networkoptix.qml 1.0
import nx.client.core 1.0
import nx.client.desktop 1.0

Rectangle
{
    id: videoContainer

    property string cameraResourceId: ""
    property CameraMotionHelper cameraMotionHelper: null
    property int currentSensitivity: 5
    property var sensitivityColors

    readonly property real kMotionGridOpacity: 0.06
    readonly property real kMotionHighlightOpacity: 0.75
    readonly property real kMotionRegionOpacity: 0.3
    readonly property real kSelectionOpacity: 0.2

    property int maxTextureSize: -1

    color: ColorTheme.window

    MediaResourceHelper
    {
        id: helper
        resourceId: cameraResourceId
    }

    VideoPositioner
    {
        id: content
        anchors.fill: videoContainer

        sourceSize: Qt.size(video.implicitWidth, video.implicitHeight)

        item: video

        customAspectRatio:
        {
            var aspectRatio = helper ? helper.customAspectRatio : 0.0
            if (aspectRatio === 0.0)
            {
                if (player)
                    aspectRatio = player.aspectRatio
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

            MouseArea
            {
                id: mouseArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.CrossCursor

                readonly property rect selectionRect: Qt.rect(
                    Math.min(x1, x2), Math.min(y1, y2), Math.abs(x2 - x1), Math.abs(y2 - y1))

                signal selectionFinished()
                signal singleClicked(real x, real y)

                property int x1: 0
                property int y1: 0
                property int x2: 0
                property int y2: 0
                property bool selectionStarted: false
                property bool movedSinceLastPress: false

                function startSelection(x, y)
                {
                    x1 = x2 = x
                    y1 = y2 = y
                    selectionStarted = true
                }

                function continueSelection(x, y)
                {
                    x2 = x
                    y2 = y
                }

                function stopSelection()
                {
                    x1 = x2 = 0
                    y1 = y2 = 0
                    selectionStarted = false
                }

                onPressed:
                {
                    movedSinceLastPress = false
                    if (mouse.button == Qt.LeftButton)
                        startSelection(mouse.x, mouse.y)
                    else if (mouse.button == Qt.RightButton)
                        stopSelection()
                }

                onReleased:
                {
                    if (selectionStarted && mouse.button == Qt.LeftButton)
                    {
                        continueSelection(mouse.x, mouse.y)
                        if (movedSinceLastPress)
                            selectionFinished()
                        else
                            singleClicked(mouse.x, mouse.y)

                        stopSelection()
                    }
                }

                onPositionChanged:
                {
                    movedSinceLastPress = true
                    if (selectionStarted)
                        continueSelection(mouse.x, mouse.y)
                }
            }

            channelOverlay: Item
            {
                id: channelOverlay
                property int index: 0

                readonly property rect selectionRect:
                {
                    var mapped = mapFromItem(mouseArea,
                        mouseArea.selectionRect.x,
                        mouseArea.selectionRect.y,
                        mouseArea.selectionRect.width,
                        mouseArea.selectionRect.height)

                    var x = MathUtils.bound(0, mapped.left, width);
                    var y = MathUtils.bound(0, mapped.top, height);

                    return Qt.rect(x, y,
                        MathUtils.bound(x, mapped.right, width) - x,
                        MathUtils.bound(y, mapped.bottom, height) - y);
                }

                readonly property bool selectionValid: mouseArea.selectionStarted
                    && selectionRect.width > 0 && selectionRect.height > 0

                readonly property rect selectionMotionRect: itemToMotionRect(selectionRect)

                readonly property vector2d motionCellSize:
                    Qt.vector2d(width / MotionDefs.gridWidth, height / MotionDefs.gridHeight)

                function motionToItemRect(motionRect)
                {
                    return Qt.rect(
                        motionRect.x * motionCellSize.x,
                        motionRect.y * motionCellSize.y,
                        motionRect.width * motionCellSize.x,
                        motionRect.height * motionCellSize.y)
                }

                function itemToMotionRect(itemRect)
                {
                    var p1 = itemToMotionPoint(itemRect.left, itemRect.top)
                    var p2 = itemToMotionPoint(itemRect.right, itemRect.bottom)
                    return Qt.rect(p1.x, p1.y, p2.x - p1.x + 1, p2.y - p1.y + 1)
                }

                function itemToMotionPoint(x, y)
                {
                    return Qt.point(
                        MathUtils.bound(0, Math.floor(x / motionCellSize.x), MotionDefs.gridWidth - 1),
                        MathUtils.bound(0, Math.floor(y / motionCellSize.y), MotionDefs.gridHeight - 1))
                }

                Rectangle
                {
                    id: selectionMarker

                    readonly property rect currentRect: motionToItemRect(selectionMotionRect)

                    x: currentRect.x
                    y: currentRect.y
                    width: currentRect.width
                    height: currentRect.height

                    color: "white"
                    opacity: kSelectionOpacity
                    visible: selectionValid
                }

                MotionRegions
                {
                    id: motionRegions
                    anchors.fill: parent
                    motionHelper: cameraMotionHelper
                    channel: index
                    fillOpacity: kMotionRegionOpacity
                    borderColor: ColorTheme.window
                    labelsColor: ColorTheme.colors.dark1
                    sensitivityColors: videoContainer.sensitivityColors

                    Connections
                    {
                        target: mouseArea

                        onSelectionFinished:
                        {
                            if (selectionValid && cameraMotionHelper)
                            {
                                cameraMotionHelper.addRect(index, currentSensitivity,
                                    selectionMotionRect)
                            }
                        }

                        onSingleClicked:
                        {
                            if (!cameraMotionHelper)
                                return

                            var point = motionRegions.mapFromItem(mouseArea, x, y)
                            if (Geometry.contains(
                                Qt.rect(0, 0, motionRegions.width, motionRegions.height), point))
                            {
                                var gridPoint = itemToMotionPoint(point.x, point.y)
                                cameraMotionHelper.fillRegion(index, currentSensitivity, gridPoint)
                            }
                        }
                    }
                }

                UniformGrid
                {
                    id: grid
                    anchors.fill: parent
                    opacity: kMotionGridOpacity
                    color: ColorTheme.colors.light1
                    cellCountX: MotionDefs.gridWidth
                    cellCountY: MotionDefs.gridHeight
                }

                MaskedUniformGrid
                {
                    id: motionHighlight
                    anchors.fill: parent
                    opacity: kMotionHighlightOpacity
                    color: ColorTheme.colors.red_l2
                    cellCountX: MotionDefs.gridWidth
                    cellCountY: MotionDefs.gridHeight

                    maskTextureProvider: MotionMaskItem
                    {
                        id: motionMaskItem

                        Connections
                        {
                            target: motionProvider

                            onMotionMaskChanged:
                            {
                                motionMaskItem.motionMask = motionProvider.motionMask(index)
                            }
                        }
                    }
                }
            }
        }
    }

    onVisibleChanged: updatePlayingState()

    MediaPlayer
    {
        id: player
        resourceId: cameraResourceId
        maxTextureSize: videoContainer.maxTextureSize

        onResourceIdChanged: video.clear()
        onSourceChanged: videoContainer.updatePlayingState()
    }

    MediaPlayerMotionProvider
    {
        id: motionProvider
        mediaPlayer: player
    }

    function updatePlayingState()
    {
        if (visible)
            player.playLive()
        else
            player.pause()
    }
}

import QtQuick 2.6

import Nx.Items 1.0
import Nx.Motion 1.0

import nx.client.core 1.0

CameraDisplay
{
    id: motionSettingsItem
    audioEnabled: false

    property CameraMotionHelper cameraMotionHelper: null
    property int currentSensitivity: 5
    property var sensitivityColors

    SelectionMouseArea
    {
        id: selectionMouseArea
        parent: videoOverlay
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
    }

    videoOverlayComponent: Item {}

    channelOverlayComponent: Item
    {
        id: channelOverlay
        property int index: 0

        MotionRegionsOverlay
        {
            id: motionRegionsOverlay

            anchors.fill: parent
            channelIndex: channelOverlay.index
            cameraMotionHelper: motionSettingsItem.cameraMotionHelper
            sensitivityColors: motionSettingsItem.sensitivityColors

            selectionRect: mapFromItem(selectionMouseArea,
                    selectionMouseArea.selectionRect.x,
                    selectionMouseArea.selectionRect.y,
                    selectionMouseArea.selectionRect.width,
                    selectionMouseArea.selectionRect.height)

            selectionRectVisible: selectionMouseArea.selectionInProgress
                && selectionRect.width > 0 && selectionRect.height > 0

            Connections
            {
                target: selectionMouseArea

                onSelectionFinished:
                {
                    if (motionRegionsOverlay.selectionRectVisible && cameraMotionHelper)
                    {
                        cameraMotionHelper.addRect(motionRegionsOverlay.channelIndex,
                            currentSensitivity, motionRegionsOverlay.selectionMotionRect)
                    }
                }

                onSingleClicked:
                {
                    if (!cameraMotionHelper)
                        return

                    var point = mapFromItem(selectionMouseArea, x, y)
                    if (Geometry.contains(Qt.rect(0, 0, width, height), point))
                    {
                        cameraMotionHelper.fillRegion(motionRegionsOverlay.channelIndex,
                            currentSensitivity, motionRegionsOverlay.itemToMotionPoint(
                                point.x, point.y))
                    }
                }
            }
        }

        MotionOverlay
        {
            anchors.fill: parent
            channelIndex: channelOverlay.index
            motionProvider: mediaPlayerMotionProvider
        }
    }

    MediaPlayerMotionProvider
    {
        id: mediaPlayerMotionProvider
        mediaPlayer: motionSettingsItem.mediaPlayer
    }

    onVisibleChanged: updatePlayingState()

    Connections
    {
        target: motionSettingsItem.mediaPlayer
        onSourceChanged: motionSettingsItem.updatePlayingState()
    }

    function updatePlayingState()
    {
        if (visible)
            mediaPlayer.playLive()
        else
            mediaPlayer.pause()
    }
}

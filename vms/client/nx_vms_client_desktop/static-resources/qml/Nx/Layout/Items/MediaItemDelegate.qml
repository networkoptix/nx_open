import QtQuick 2.6

import Nx.Core 1.0
import Nx.Items 1.0
import Nx.Motion 1.0

import nx.client.core 1.0

ResourceItemDelegate
{
    property alias motionAllowed: motionSearchButton.checked

    rotationAllowed: true

    contentItem: CameraDisplay
    {
        id: cameraDisplay
        anchors.fill: parent
        cameraResourceId: layoutItemData ? layoutItemData.resourceId.toString() : null
        audioEnabled: true

        channelOverlayComponent: Item
        {
            id: channelOverlay
            property int index: 0

            MotionOverlay
            {
                visible: motionAllowed
                anchors.fill: parent
                channelIndex: channelOverlay.index
                motionProvider: mediaPlayerMotionProvider
            }
        }

        MediaPlayerMotionProvider
        {
            id: mediaPlayerMotionProvider
            mediaPlayer: cameraDisplay.mediaPlayer
        }

        Component.onCompleted: mediaPlayer.playLive()
    }

    titleBar.leftContent.children:
    [
        RecordingStatusIndicator
        {
            anchors.verticalCenter: parent.verticalCenter
            cameraId: layoutItemData.resource.id
        }
    ]

    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/image_enhancement.png"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/zoom_window.png"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/ptz.png"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/fisheye.png"
            checkable: true
        },

        TitleBarButton
        {
            id: motionSearchButton
            iconUrl: "qrc:/skin/item/search.png"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/screenshot.png"
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/rotate.png"

            onPressed: resourceItem.rotationInstrument.start(mousePosition, this)
            onMousePositionChanged: resourceItem.rotationInstrument.move(mousePosition, this)
            onReleased: resourceItem.rotationInstrument.stop()
            onCanceled: resourceItem.rotationInstrument.stop()
        },

        TitleBarButton
        {
            id: infoButton

            iconUrl: "qrc:/skin/item/info.png"
            checkable: true

            Binding
            {
                target: infoButton
                property: "checked"
                value: layoutItemData.displayInfo
            }
            // Use Action from Qt 5.10
            onCheckedChanged: layoutItemData.displayInfo = checked
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/close.png"
            onClicked: layoutItemData.layout.removeItem(layoutItemData.itemId)
        }
    ]
}

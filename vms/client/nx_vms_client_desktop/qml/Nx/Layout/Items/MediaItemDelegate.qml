// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Items
import Nx.Motion

import nx.vms.client.core

ResourceItemDelegate
{
    property alias motionAllowed: motionSearchButton.checked

    overlayParent: cameraDisplay.videoOverlay

    rotationAllowed: true

    contentItem: CameraDisplay
    {
        id: cameraDisplay
        anchors.fill: parent
        cameraResource: layoutItemData ? layoutItemData.resource : null
        audioEnabled: true
        color: "transparent"

        tag: "NewScene"

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

        videoOverlayComponent: Item
        {
            StatusOverlay
            {
                anchors.fill: parent
                resource: layoutItemData.resource
                mediaPlayer: cameraDisplay.mediaPlayer
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
            resource: layoutItemData.resource
        }
    ]

    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/image_enhancement_24.svg"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/zoom_window.svg"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/ptz.svg"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/fisheye.svg"
            checkable: true
        },

        TitleBarButton
        {
            id: motionSearchButton
            iconUrl: "image://svg/skin/item/motion.svg"
            checkable: true
        },

        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/screenshot.svg"
        },

        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/rotate.svg"

            onPressed: resourceItem.rotationInstrument.start(mousePosition, this)
            onMousePositionChanged: resourceItem.rotationInstrument.move(mousePosition, this)
            onReleased: resourceItem.rotationInstrument.stop()
            onCanceled: resourceItem.rotationInstrument.stop()
        },

        TitleBarButton
        {
            id: infoButton

            iconUrl: "image://svg/skin/item/info.svg"
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
            iconUrl: "image://svg/skin/item/close.svg"
            onClicked: layoutItemData.layout.removeItem(layoutItemData.itemId)
        }
    ]
}

import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "../../Screens/private/VideoScreen"

Item
{
    id: cameraDisplay

    property alias resourceId: videoScreenController.resourceId
    property LiteClientLayoutHelper layoutHelper: null
    property QnCameraListModel camerasModel: null

    property alias useClickEffect: materialEffect.visible

    property alias videoScreenController: videoScreenController
    property alias thumbnailCacheAccessor: thumbnailCacheAccessor

    property alias cameraWarningVisible: d.cameraWarningVisible

    signal nextCameraRequested()
    signal previousCameraRequested()
    signal clicked()
    signal doubleClicked()
    signal activityDetected()

    VideoScreenController
    {
        id: videoScreenController

        mediaPlayer.onPlayingChanged:
        {
            if (mediaPlayer.playing)
            {
                screenshot.source = ""
                screenshotSourceBinding.when = false
            }
        }

        mediaPlayer.onSourceChanged: videoOutput.clear()
    }

    QnThumbnailCacheAccessor
    {
        id: thumbnailCacheAccessor
        resourceId: cameraDisplay.resourceId
        onResourceIdChanged:
        {
            refreshThumbnail()
            screenshotSourceBinding.when = true
        }
    }

    Object
    {
        id: d

        property bool cameraWarningVisible:
            (videoScreenController.offline
                || videoScreenController.cameraUnauthorized
                || videoScreenController.failed
                || videoScreenController.noVideoStreams)
            && !videoScreenController.mediaPlayer.playing
    }

    VideoPositioner
    {
        id: video

        anchors.fill: parent
        visible: !dummyLoader.visible && !screenshot.visible

        item: videoOutput

        sourceSize: Qt.size(videoOutput.implicitWidth, videoOutput.implicitHeight)
        videoRotation: videoScreenController.resourceHelper.customRotation

        customAspectRatio:
        {
            var aspectRatio = videoScreenController.resourceHelper.customAspectRatio
            if (aspectRatio === 0.0)
            {
                if (videoScreenController.mediaPlayer)
                    aspectRatio = videoScreenController.mediaPlayer.aspectRatio
                else
                    aspectRatio = sourceSize.width / sourceSize.height
            }

            var layoutSize = videoScreenController.resourceHelper.layoutSize
            aspectRatio *= layoutSize.width / layoutSize.height

            return aspectRatio
        }

        MultiVideoOutput
        {
            id: videoOutput
            mediaPlayer: videoScreenController.mediaPlayer
            resourceHelper: videoScreenController.resourceHelper
        }
    }

    Image
    {
        id: screenshot
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        visible: status === Image.Ready

        Binding
        {
            id: screenshotSourceBinding
            target: screenshot
            property: "source"
            value: thumbnailCacheAccessor.thumbnailUrl
        }
    }

    WheelSwitchArea
    {
        anchors.fill: parent
        onPreviousRequested: previousCameraRequested()
        onNextRequested: nextCameraRequested()
        maxConsequentRequests: camerasModel ? camerasModel.count : 0
    }

    Loader
    {
        id: dummyLoader
        anchors.fill: parent
        visible: active
        sourceComponent: Component
        {
            VideoDummy
            {
                width: cameraDisplay.width
                state: videoScreenController.dummyState
            }
        }

        active: d.cameraWarningVisible
    }

    Loader
    {
        id: noCameraDummyLoader
        anchors.fill: parent
        visible: active
        sourceComponent: SelectCameraDummy {}
        active: resourceId === ""
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true

        onClicked: cameraDisplay.clicked()
        onDoubleClicked: cameraDisplay.doubleClicked()
        onMouseXChanged: cameraDisplay.activityDetected()
        onMouseYChanged: cameraDisplay.activityDetected()
    }

    MaterialEffect
    {
        id: materialEffect
        clip: true
        anchors.fill: parent
        mouseArea: mouseArea
        rippleSize: width * 2
    }

    Keys.onPressed:
    {
        activityDetected()

        if (event.modifiers === Qt.ControlModifier)
        {
            if (event.key === Qt.Key_Left)
            {
                previousCameraRequested()
                event.accepted = true
            }
            else if (event.key === Qt.Key_Right)
            {
                nextCameraRequested()
                event.accepted = true
            }
        }
        else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
        {
            doubleClicked()
            event.accepted = true
        }
    }
}

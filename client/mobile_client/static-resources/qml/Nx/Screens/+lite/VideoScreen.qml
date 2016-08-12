import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "../private/VideoScreen"

PageBase
{
    id: videoScreen
    objectName: "videoScreen"

    property alias resourceId: videoScreenController.resourceId
    property string initialScreenshot
    property QnLiteClientLayoutHelper layoutHelper: null
    property QnCameraListModel camerasModel: null

    signal nextCameraRequested()
    signal previousCameraRequested()

    VideoScreenController
    {
        id: videoScreenController

        mediaPlayer.onPlayingChanged:
        {
            if (mediaPlayer.playing)
                video.screenshotSource = ""
        }

        onOfflineChanged:
        {
            if (offline)
            {
                offlineStatusDelay.restart()
            }
            else
            {
                offlineStatusDelay.stop()
                d.showOfflineStatus = false
            }
        }
    }

    Object
    {
        id: d

        property bool showOfflineStatus: false
        property bool cameraWarningVisible:
            (showOfflineStatus
                || videoScreenController.cameraUnauthorized
                || videoScreenController.failed)
            && !videoScreenController.mediaPlayer.playing

        Timer
        {
            id: offlineStatusDelay
            interval: 2000
            onTriggered: d.showOfflineStatus = true
        }
    }

    ScalableVideo
    {
        id: video

        anchors.fill: parent
        visible: !dummyLoader.visible

        source: videoScreenController.mediaPlayer
        screenshotSource: initialScreenshot
        customAspectRatio: (videoScreenController.resourceHelper.customAspectRatio
            || videoScreenController.mediaPlayer.aspectRatio)
        videoRotation: videoScreenController.resourceHelper.customRotation
    }

    Loader
    {
        id: dummyLoader
        anchors.centerIn: parent
        visible: d.cameraWarningVisible
        sourceComponent: dummyComponent
        active: visible
    }

    Component
    {
        id: dummyComponent

        VideoDummy
        {
            width: videoScreen.width
            state: videoScreenController.dummyState
        }
    }

    Keys.onPressed:
    {
        if (event.modifiers == Qt.ControlModifier)
        {
            if (event.key == Qt.Key_Left)
            {
                nextCameraRequested()
                event.accepted = true
            }
            else if (event.key == Qt.Key_Right)
            {
                previousCameraRequested()
                event.accepted = true
            }
        }
        else if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return)
        {
            Workflow.popCurrentScreen()
            event.accepted = true
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onWheel:
        {
            if (wheel.angleDelta.y > 0)
                previousCameraRequested()
            else
                nextCameraRequested()
        }
    }

    onNextCameraRequested:
    {
        layoutHelper.singleCameraId = camerasModel.nextResourceId(resourceId)
    }
    onPreviousCameraRequested:
    {
        layoutHelper.singleCameraId = camerasModel.previousResourceId(resourceId)
    }
}

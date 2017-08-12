import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Items.LiteClient 1.0
import com.networkoptix.qml 1.0

import "../private/VideoScreen"
import "private/VideoScreen"

PageBase
{
    id: videoScreen
    objectName: "videoScreen"

    property alias resourceId: videoScreenController.resourceId
    property string initialScreenshot
    property LiteClientLayoutHelper layoutHelper: null
    property QnCameraListModel camerasModel: null

    signal nextCameraRequested()
    signal previousCameraRequested()

    VideoScreenController
    {
        id: videoScreenController

        mediaPlayer.onPlayingChanged:
        {
            if (mediaPlayer.playing)
                screenshot.source = ""
        }

        mediaPlayer.onSourceChanged:
        {
            video.clear()

            if (mediaPlayer.source && !transformationsWarningLoader.active)
                mediaPlayer.playLive()
            else
                mediaPlayer.stop()
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

    QnThumbnailCacheAccessor
    {
        id: thumbnailCacheAccessor
        resourceId: videoScreen.resourceId
        onResourceIdChanged: refreshThumbnail()
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
        visible: !dummyLoader.visible && !screenshot.visible

        mediaPlayer: videoScreenController.mediaPlayer
        resourceHelper: videoScreenController.resourceHelper
    }

    Image
    {
        id: screenshot
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        visible: status == Image.Ready
        source: thumbnailCacheAccessor.thumbnailUrl
    }

    WheelSwitchArea
    {
        anchors.fill: parent
        onPreviousRequested: previousCameraRequested()
        onNextRequested: nextCameraRequested()
        maxConsequentRequests: camerasModel ? camerasModel.count : 0
    }

    MouseArea
    {
        anchors.fill: parent
        onDoubleClicked: Workflow.popCurrentScreen()
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
                width: videoScreen.width
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

    Loader
    {
        id: transformationsWarningLoader
        anchors.fill: parent
        visible: active
        active: false

        sourceComponent: TransformationsNotSupportedWarning
        {
            onCloseClicked:
            {
                transformationsWarningLoader.active = false
                videoScreenController.start()
            }
        }
    }

    Keys.onPressed:
    {
        if (event.modifiers == Qt.ControlModifier)
        {
            if (event.key == Qt.Key_Left)
            {
                previousCameraRequested()
                event.accepted = true
            }
            else if (event.key == Qt.Key_Right)
            {
                nextCameraRequested()
                event.accepted = true
            }
        }
        else if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return)
        {
            Workflow.popCurrentScreen()
            event.accepted = true
        }
    }

    onNextCameraRequested:
    {
        layoutHelper.singleCameraId = camerasModel.nextResourceId(resourceId)
        showTransformationsWarningIfNeeded()
    }
    onPreviousCameraRequested:
    {
        layoutHelper.singleCameraId = camerasModel.previousResourceId(resourceId)
        showTransformationsWarningIfNeeded()
    }

    onActivePageChanged:
    {
        if (activePage && !transformationsWarningLoader.active)
            videoScreenController.start()
    }

    onResourceIdChanged: hideTransformationsWarning()

    function hideTransformationsWarning()
    {
        transformationsWarningLoader.active = false
    }

    function showTransformationsWarningIfNeeded()
    {
        transformationsWarningLoader.active =
            resourceId !== ""
                && !d.cameraWarningVisible
                && (videoScreenController.resourceHelper.customRotation !== 0
                    || videoScreenController.resourceHelper.customAspectRatio !== 0.0)
    }
}

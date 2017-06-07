import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "../private/VideoScreen"

PageBase
{
    id: videoScreen
    objectName: "videoScreen"

    property alias resourceId: videoScreenController.resourceId
    property alias initialScreenshot: screenshot.source
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
            if (mediaPlayer.source)
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
        sourceComponent: Rectangle
        {
            color: ColorTheme.base3

            Column
            {
                width: parent.width
                anchors.centerIn: parent

                Text
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Select camera")
                    color: ColorTheme.base13
                    font.pixelSize: 24
                    font.capitalization: Font.AllUppercase
                    wrapMode: Text.WordWrap
                }

                Text
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Press Ctrl + Arrow or use mouse wheel")
                    color: ColorTheme.base13
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        active: resourceId == ""
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

    WheelSwitchArea
    {
        anchors.fill: parent
        onPreviousRequested: previousCameraRequested()
        onNextRequested: nextCameraRequested()
        maxConsequentRequests: camerasModel ? camerasModel.count - 1 : 0
    }

    MouseArea
    {
        anchors.fill: parent
        onDoubleClicked: Workflow.popCurrentScreen()
    }

    onResourceIdChanged: video.clear()

    onNextCameraRequested:
    {
        layoutHelper.singleCameraId = camerasModel.nextResourceId(resourceId)
    }
    onPreviousCameraRequested:
    {
        layoutHelper.singleCameraId = camerasModel.previousResourceId(resourceId)
    }
}

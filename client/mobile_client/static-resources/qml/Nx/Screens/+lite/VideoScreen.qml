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

    property alias resourceId: cameraDisplay.resourceId
    property string initialScreenshot
    property alias layoutHelper: cameraDisplay.layoutHelper
    property alias camerasModel: cameraDisplay.camerasModel

    signal nextCameraRequested()
    signal previousCameraRequested()

    CameraDisplay
    {
        id: cameraDisplay

        anchors.fill: parent

        useClickEffect: false

        onNextCameraRequested:
        {
            layoutHelper.singleCameraId = camerasModel.nextResourceId(resourceId)
            videoScreen.start()
        }
        onPreviousCameraRequested:
        {
            layoutHelper.singleCameraId = camerasModel.previousResourceId(resourceId)
            videoScreen.start()
        }

        onDoubleClicked: Workflow.popCurrentScreen()

        Component.onCompleted: forceActiveFocus()
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
                cameraDisplay.videoScreenController.start()
                cameraDisplay.forceActiveFocus()
            }

            Keys.forwardTo: cameraDisplay
        }
    }

    onActivePageChanged:
    {
        if (activePage && !transformationsWarningLoader.active)
            cameraDisplay.videoScreenController.start()
    }

    onResourceIdChanged: hideTransformationsWarning()

    function hideTransformationsWarning()
    {
        transformationsWarningLoader.active = false
        cameraDisplay.forceActiveFocus()
    }

    function start()
    {
        var resourceHelper = cameraDisplay.videoScreenController.resourceHelper

        transformationsWarningLoader.active =
            resourceId !== ""
                && !cameraDisplay.cameraWarningVisible
                && (resourceHelper.customRotation !== 0
                    || resourceHelper.customAspectRatio !== 0.0)

        if (transformationsWarningLoader.active)
        {
            cameraDisplay.videoScreenController.stop()
        }
        else
        {
            cameraDisplay.videoScreenController.start()
            cameraDisplay.forceActiveFocus()
        }
    }
}

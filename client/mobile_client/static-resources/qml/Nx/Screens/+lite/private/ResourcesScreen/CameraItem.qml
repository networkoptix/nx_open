import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Media 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Items.LiteClient 1.0
import com.networkoptix.qml 1.0

Control
{
    id: cameraItem

    property alias resourceId: cameraDisplay.resourceId
    property bool paused: false
    property LiteClientLayoutHelper layoutHelper: null
    property int layoutX: -1
    property int layoutY: -1

    property bool active: false

    signal clicked()
    signal doubleClicked()
    signal activityDetected()
    signal nextCameraRequested()
    signal previousCameraRequested()

    background: Rectangle
    {
        color: ColorTheme.windowBackground
    }

    contentItem: Rectangle
    {
        id: mainContainer

        width: parent.availableWidth
        height: parent.availableHeight
        color: resourceId && cameraDisplay.videoScreenController.offline
            ? ColorTheme.base7
            : ColorTheme.base4

        CameraDisplay
        {
            id: cameraDisplay

            anchors.fill: parent

            camerasModel: camerasModel

            videoScreenController.mediaPlayer.allowOverlay: false
            videoScreenController.mediaPlayer.videoQuality: MediaPlayer.LowVideoQuality

            onNextCameraRequested: cameraItem.nextCameraRequested()
            onPreviousCameraRequested: cameraItem.previousCameraRequested()
            onClicked: cameraItem.clicked()
            onDoubleClicked: cameraItem.doubleClicked()
            onActivityDetected: cameraItem.activityDetected()

            videoScreenController.mediaPlayer.onSourceChanged:
            {
                if (videoScreenController.mediaPlayer.source)
                    videoScreenController.start()
                else
                    videoScreenController.stop()
            }
        }

        Rectangle
        {
            id: controls

            anchors.fill: parent

            opacity: 0.0
            SequentialAnimation
            {
                id: controlsFadeOutAnimation

                PauseAnimation { duration: 3000 }

                NumberAnimation
                {
                    target: controls
                    property: "opacity"
                    duration: 1000
                    to: 0.0
                }
            }

            z: 5

            border.color: ColorTheme.brand_main
            border.width: 2
            color: "transparent"

            Item
            {
                anchors.fill: parent
                anchors.margins: parent.border.width

                Rectangle
                {
                    width: parent.width
                    height: 24
                    color: ColorTheme.transparent(ColorTheme.base5, 0.6)
                    visible: resourceId

                    Text
                    {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        text: cameraDisplay.videoScreenController.resourceHelper.resourceName
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                        color: ColorTheme.windowText
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }

    Connections
    {
        target: layoutHelper
        onCameraIdChanged:
        {
            if (layoutX === x && layoutY === y)
                cameraItem.resourceId = resourceId
        }
        onLayoutChanged: updateResourceId()
    }

    onPausedChanged:
    {
        if (cameraItem.paused)
            cameraDisplay.videoScreenController.stop()
        else
            cameraDisplay.videoScreenController.start()
    }

    onLayoutHelperChanged: updateResourceId()
    Component.onCompleted: updateResourceId()
    onResourceIdChanged: activityDetected()

    onActiveChanged:
    {
        if (active)
            showControls()
        else
            controls.opacity = 0
    }

    function showControls()
    {
        if (active)
        {
            controls.opacity = 1.0
            cameraDisplay.forceActiveFocus()
        }

        controlsFadeOutAnimation.restart()
    }

    function updateResourceId()
    {
        if (layoutX < 0 || layoutY < 0 || !layoutHelper)
            return

        resourceId = layoutHelper.cameraIdOnCell(layoutX, layoutY)
    }
}

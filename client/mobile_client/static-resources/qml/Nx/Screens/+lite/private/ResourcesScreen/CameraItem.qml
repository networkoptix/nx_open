import QtQuick 2.6
import QtMultimedia 5.5
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Control
{
    id: cameraItem

    property alias resourceId: resourceHelper.resourceId
    property bool paused: false

    QnMediaResourceHelper
    {
        id: resourceHelper
    }

    QtObject
    {
        id: d

        property bool offline: resourceHelper.resourceStatus == QnMediaResourceHelper.Offline ||
                               resourceHelper.resourceStatus == QnMediaResourceHelper.NotDefined ||
                               resourceHelper.resourceStatus == QnMediaResourceHelper.Unauthorized
        property bool unauthorized: resourceHelper.resourceStatus == QnMediaResourceHelper.Unauthorized
    }

    background: Rectangle
    {
        color: ColorTheme.windowBackground
    }

    contentItem: Rectangle
    {
        id: thumbnailContainer

        width: parent.availableWidth
        height: parent.availableHeight
        color: resourceId && d.offline ? ColorTheme.base7 : ColorTheme.base4

        Loader
        {
            id: thumbnailContentLoader

            anchors.centerIn: parent
            sourceComponent:
            {
                if (!resourceId)
                    return undefined
                if (d.offline || d.unauthorized)
                    return dummyComponent
                else
                    return videoComponent
            }
        }
    }

    Component
    {
        id: dummyComponent

        Column
        {
            width: parent ? parent.width : 0
            leftPadding: 8
            rightPadding: 8

            Image
            {
                anchors.horizontalCenter: parent.horizontalCenter
                source: d.unauthorized ? lp("/images/camera_locked.png") : lp("/images/camera_offline.png")
            }

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - parent.leftPadding - parent.rightPadding
                height: 32
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                text: d.unauthorized ? qsTr("Authentication\nrequired") : qsTr("Offline")
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                font.pixelSize: 14
                font.weight: Font.Normal
                color: ColorTheme.windowText
            }
        }
    }

    Component
    {
        id: videoComponent

        Item
        {
            width: thumbnailContainer.width
            height: thumbnailContainer.height

            Video
            {
                id: videoOutput

                anchors.fill: parent
                source: player
                aspectRatio: resourceHelper.aspectRatio
                videoRotation: resourceHelper.rotation

                QnScenePositionListener
                {
                    item: videoOutput
                    onScenePosChanged:
                    {
                        player.videoGeometry = Qt.rect(
                            scenePos.x,
                            scenePos.y,
                            videoOutput.width,
                            videoOutput.height)
                    }
                }
            }

            MediaPlayer
            {
                id: player

                resourceId: cameraItem.resourceId
                Component.onCompleted: playLive()
                videoQuality: QnPlayer.LowVideoQuality
            }

            Connections
            {
                target: cameraItem
                onPausedChanged:
                {
                    if (cameraItem.paused)
                        player.stop()
                    else
                        player.play()
                }
            }

            MouseArea
            {
                id: mouseArea
                anchors.fill: parent
                onClicked: Workflow.openVideoScreen(resourceId)
            }

            MaterialEffect
            {
                clip: true
                anchors.fill: parent
                mouseArea: mouseArea
                rippleSize: width * 2
            }
        }
    }

    Keys.onReturnPressed: mouseArea.clicked()
}

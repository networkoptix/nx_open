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
    property QnLiteClientLayoutHelper layoutHelper: null
    property int layoutX: -1
    property int layoutY: -1

    property bool active: false

    signal clicked()
    signal activityDetected()
    signal nextCameraRequested()
    signal previousCameraRequested()

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
                    return noCameraComponent
                if (d.offline || d.unauthorized)
                    return dummyComponent
                else
                    return videoComponent
            }
        }

        Rectangle
        {
            id: controls

            anchors.fill: parent

            opacity: 0.0
            Behavior on opacity { NumberAnimation { duration: 200 } }
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
                        text: resourceHelper.resourceName
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

    Component
    {
        id: noCameraComponent

        Rectangle
        {
            width: cameraItem.width
            height: cameraItem.height

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

                text: d.unauthorized ? qsTr("Authentication required") : qsTr("Offline")
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

            VideoPositioner
            {
                anchors.fill: parent
                customAspectRatio: resourceHelper.customAspectRatio || player.aspectRatio
                videoRotation: resourceHelper.customRotation
                sourceSize: Qt.size(videoOutput.sourceRect.width, videoOutput.sourceRect.height)

                item: VideoOutput
                {
                    id: videoOutput

                    source: player
                    fillMode: VideoOutput.Stretch

                    QnScenePositionListener
                    {
                        item: parent
                        onScenePosChanged:
                        {
                            player.videoGeometry = Qt.rect(
                                scenePos.x,
                                scenePos.y,
                                parent.width,
                                parent.height)
                        }
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
        }
    }

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        onClicked: cameraItem.clicked()
        hoverEnabled: true

        onWheel:
        {
            activityDetected()
            if (wheel.angleDelta.y > 0)
                previousCameraRequested()
            else
                nextCameraRequested()
        }

        onMouseXChanged: activityDetected()
        onMouseYChanged: activityDetected()
    }

    MaterialEffect
    {
        clip: true
        anchors.fill: parent
        mouseArea: mouseArea
        rippleSize: width * 2
    }

    Timer
    {
        id: fadeOutTimer
        interval: 3000
        onTriggered:
        {
            controls.opacity = 0.0
        }
    }

    Connections
    {
        target: layoutHelper
        onCameraIdChanged:
        {
            if (layoutX == x && layoutY == y)
                cameraItem.resourceId = resourceId
        }
        onLayoutChanged: updateResourceId()
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
            controls.opacity = 1.0
        fadeOutTimer.restart()
    }

    function updateResourceId()
    {
        if (layoutX < 0 || layoutY < 0 || !layoutHelper)
            return

        resourceId = layoutHelper.cameraIdOnCell(layoutX, layoutY)
    }

    Keys.onPressed:
    {
        activityDetected()

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
        else if (event.key == Qt.Key_Return || event.key == Qt.Key_Enter)
        {
            clicked()
            event.accepted = true
        }
    }
}

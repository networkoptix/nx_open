import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Media 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Settings 1.0
import com.networkoptix.qml 1.0

Control
{
    id: cameraItem

    property alias text: label.text
    property string thumbnail
    property int status
    property bool keepStatus: false
    property alias resourceId: resourceHelper.resourceId

    property bool active: false

    signal clicked()
    signal thumbnailRefreshRequested()

    padding: 4
    implicitWidth: 200
    implicitHeight: contentItem.implicitHeight + 2 * padding

    QtObject
    {
        id: d

        property int status: QnCameraListModel.Offline

        property bool offline: status == QnCameraListModel.Offline ||
                               status == QnCameraListModel.NotDefined ||
                               status == QnCameraListModel.Unauthorized
        property bool unauthorized: status == QnCameraListModel.Unauthorized

        // This property prevents video component re-creation while scrolling.
        property bool videoAllowed: false
    }

    MediaResourceHelper
    {
        id: resourceHelper
    }

    Binding
    {
        target: d
        property: "status"
        value: status
        when: !keepStatus
    }

    background: Rectangle
    {
        color: ColorTheme.windowBackground
    }

    contentItem: Column
    {
        id: contentColumn

        spacing: 8
        width: parent.availableWidth
        anchors.centerIn: parent

        Rectangle
        {
            id: thumbnailContainer

            width: parent.width
            height: parent.width * 9 / 16
            color: d.offline ? ColorTheme.base7 : ColorTheme.base4

            Loader
            {
                id: thumbnailContentLoader

                anchors.centerIn: parent
                sourceComponent:
                {
                    if (d.offline)
                        return thumbnailDummyComponent

                    if (!cameraItem.active || !d.videoAllowed || !settings.liveVideoPreviews)
                    {
                        if (!cameraItem.thumbnail)
                            return thumbnailPreloaderComponent

                        return thumbnailComponent
                    }

                    return videoComponent
                }
            }
        }

        Row
        {
            width: parent.width
            spacing: 6

            StatusIndicator
            {
                id: statusIndicator

                status: d.status
                y: 4
            }

            Text
            {
                id: label

                width: parent.width - x - parent.spacing
                height: 24
                font.pixelSize: 16
                font.weight: d.offline ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                color: d.offline ? ColorTheme.base11 : ColorTheme.windowText
            }
        }
    }

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        onClicked: cameraItem.clicked()
    }

    MaterialEffect
    {
        clip: true
        anchors.fill: parent
        mouseArea: mouseArea
        rippleSize: width * 2
    }

    Component
    {
        id: thumbnailDummyComponent

        Column
        {
            width: thumbnailContainer.width
            leftPadding: 8
            rightPadding: 8
            property real availableWidth: width - leftPadding - rightPadding

            Image
            {
                anchors.horizontalCenter: parent.horizontalCenter
                source: d.unauthorized
                    ? lp("/images/camera_locked.png")
                    : lp("/images/camera_offline.png")
            }

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.availableWidth - 32
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
        id: thumbnailPreloaderComponent

        ThreeDotBusyIndicator {}
    }

    Component
    {
        id: thumbnailComponent

        Image
        {
            source: cameraItem.thumbnail
            width: thumbnailContainer.width
            height: thumbnailContainer.height
            fillMode: Qt.KeepAspectRatio
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
                id: video

                anchors.fill: parent
                customAspectRatio: resourceHelper.customAspectRatio || mediaPlayer.aspectRatio
                videoRotation: resourceHelper.customRotation
                sourceSize: Qt.size(videoOutput.sourceRect.width, videoOutput.sourceRect.height)
                visible: mediaPlayer.playing

                item: videoOutput

                VideoOutput
                {
                    id: videoOutput
                    player: mediaPlayer
                    fillMode: VideoOutput.Stretch
                }
            }

            Image
            {
                id: image

                anchors.fill: parent
                source: cameraItem.thumbnail
                fillMode: Qt.KeepAspectRatio
                visible: !video.visible && status === Image.Ready
            }

            ThreeDotBusyIndicator
            {
                anchors.centerIn: parent
                visible: !video.visible && !image.visible
            }

            MediaPlayer
            {
                id: mediaPlayer

                resourceId: cameraItem.resourceId
                Component.onCompleted: playLive()
                videoQuality: MediaPlayer.LowIframesOnlyVideoQuality
                audioEnabled: false
            }
        }
    }

    Timer
    {
        id: refreshTimer

        readonly property int initialLoadDelay: 400
        readonly property int reloadDelay: 60 * 1000

        interval: initialLoadDelay
        repeat: true
        running: active && connectionManager.connectionState === QnConnectionManager.Ready

        onTriggered:
        {
            interval = reloadDelay
            cameraItem.thumbnailRefreshRequested()
        }

        onRunningChanged:
        {
            if (!running)
                interval = initialLoadDelay
        }
    }

    Timer
    {
        interval: 2000
        running: active
        onTriggered: d.videoAllowed = true
    }
}

import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Control
{
    id: cameraItem

    property alias text: label.text
    property alias thumbnail: thumbnail.source
    property int status

    signal clicked
    signal pressAndHold

    implicitWidth: 200
    implicitHeight: 48

    QtObject
    {
        id: d

        property bool offline: status == QnCameraListModel.Offline ||
                               status == QnCameraListModel.NotDefined ||
                               status == QnCameraListModel.Unauthorized
        property bool unauthorized: status == QnCameraListModel.Unauthorized
    }

    contentItem: Row
    {
        width: parent.availableWidth
        height: parent.availableHeight

        Rectangle
        {
            id: thumbnailContainer

            width: 80
            height: parent.height
            color: d.offline ? ColorTheme.base7 : ColorTheme.base4

            Loader
            {
                id: thumbnailContentLoader

                anchors.centerIn: parent
                sourceComponent:
                {
                    if (d.offline || d.unauthorized)
                        return thumbnailDummyComponent
                    else if (!cameraItem.thumbnail)
                        return thumbnailPreloaderComponent
                    else
                        return thumbnailComponent
                }
            }

            Image
            {
                anchors.centerIn: parent
                source: d.unauthorized ? lp("/images/camera_locked.png") : lp("/images/camera_offline.png")
                visible: d.offline
                sourceSize.width: 40
                sourceSize.height: 40
            }

            Image
            {
                id: thumbnail
                anchors.fill: parent
                fillMode: Qt.KeepAspectRatio
                asynchronous: true
                visible: !d.offline && status == Image.Ready
            }
        }

        Item
        {
            id: spacer
            width: 16
            height: 1
        }

        StatusIndicator
        {
            id: statusIndicator
            status: cameraItem.status
            anchors.verticalCenter: parent.verticalCenter
        }

        Text
        {
            id: label
            leftPadding: 6
            width: parent.width - statusIndicator.width - spacer.width - thumbnailContainer.width
            height: 48
            verticalAlignment: Text.AlignVCenter
            maximumLineCount: 1
            wrapMode: Text.WordWrap
            font.pixelSize: 16
            font.weight: d.offline ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            color: d.offline ? ColorTheme.base11 : ColorTheme.windowText
        }
    }

    Component
    {
        id: thumbnailDummyComponent

        Image
        {
            source: d.unauthorized ? lp("/images/camera_locked.png") : lp("/images/camera_offline.png")
            sourceSize.width: 40
            sourceSize.height: 40
        }
    }

    Component
    {
        id: thumbnailPreloaderComponent

        ThreeDotBusyIndicator
        {
            scale: 0.5
        }
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
        rippleSize: 160
    }
}

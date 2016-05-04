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
    property string thumbnail
    property int status

    signal clicked

    padding: 4
    implicitWidth: 200
    implicitHeight: contentItem.implicitHeight + 2 * padding

    QtObject
    {
        id: d

        property bool offline: status == QnCameraListModel.Offline ||
                               status == QnCameraListModel.NotDefined ||
                               status == QnCameraListModel.Unauthorized
        property bool unauthorized: status == QnCameraListModel.Unauthorized
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
            height: parent.width * 3 / 4
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
        }

        Row
        {
            width: parent.width
            spacing: 8

            StatusIndicator
            {
                id: statusIndicator

                status: cameraItem.status
                y: 4
            }

            Text
            {
                id: label

                width: parent.width - x - parent.spacing
                height: 24
                maximumLineCount: 1
                font.pixelSize: 16
                font.weight: d.offline ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                wrapMode: Text.Wrap
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
            width: parent.width
            leftPadding: 8
            rightPadding: 8

            Image
            {
                anchors.horizontalCenter: parent.horizontalCenter
                source: d.unauthorized ? "/images/camera_locked.png" : "/images/camera_offline.png"
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
                color: QnTheme.cameraText
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
}

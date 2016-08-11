import QtQuick 2.6
import Nx 1.0

Item
{
    id: videoDummy

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    state: ""

    Column
    {
        id: content

        width: parent.width
        height: parent.height

        Image
        {
            id: image
            width: 136
            height: 136
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text
        {
            id: text

            width: parent.width
            height: 96
            anchors.horizontalCenter: parent.horizontalCenter

            color: ColorTheme.base13

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 32
            font.weight: Font.Normal
            wrapMode: Text.WordWrap
        }
    }

    states: [
        State
        {
            name: ""
            PropertyChanges { target: image; source: "" }
            PropertyChanges { target: text; text: "" }
        },
        State
        {
            name: "serverOffline"
            PropertyChanges { target: image; source: lp("/images/server_offline_1.png") }
            PropertyChanges { target: text; text: qsTr("Server offline") }
        },
        State
        {
            name: "cameraUnauthorized"
            PropertyChanges { target: image; source: lp("/images/camera_locked_1.png") }
            PropertyChanges { target: text; text: qsTr("Authentication required") }
        },
        State
        {
            name: "cameraOffline"
            PropertyChanges { target: image; source: lp("/images/camera_offline_1.png") }
            PropertyChanges { target: text; text: qsTr("Camera offline") }
        },
        State
        {
            name: "videoLoadingFailed"
            PropertyChanges { target: image; source: lp("/images/camera_warning_1.png") }
            PropertyChanges { target: text; text: qsTr("Can't load video") }
        }
    ]
}

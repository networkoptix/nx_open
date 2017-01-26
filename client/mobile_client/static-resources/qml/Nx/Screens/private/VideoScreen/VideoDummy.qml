import QtQuick 2.6
import Nx 1.0
import Nx.Items 1.0

DummyMessage
{
    id: dummyMessage

    state: ""

    states: [
        State
        {
            name: ""
            PropertyChanges
            {
                target: dummyMessage
                title: ""
                image: ""
            }
        },
        State
        {
            name: "serverOffline"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Server offline")
                image: lp("/images/alert_server_offline.png")
            }
        },
        State
        {
            name: "cameraUnauthorized"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Authentication required")
                image: lp("/images/alert_locked.png")
            }
        },
        State
        {
            name: "cameraOffline"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Camera offline")
                image: lp("/images/alert_camera_offline.png")
            }
        },
        State
        {
            name: "videoLoadingFailed"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Can't load video")
                image: lp("/images/alert_alert.png")
            }
        }
    ]
}

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
                title: qsTr("Offline")
                image: lp("/images/lite_client/camera_offline.png")
            }
        },
        State
        {
            name: "cameraUnauthorized"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Unauthorized")
                image: lp("/images/lite_client/camera_locked.png")
            }
        },
        State
        {
            name: "cameraOffline"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Offline")
                image: lp("/images/lite_client/camera_offline.png")
            }
        },
        State
        {
            name: "videoLoadingFailed"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Cannot load video")
                image: lp("/images/lite_client/alert_alert.png")
            }
        },
        State
        {
            name: "noVideoStreams"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Cannot play any video stream")
                image: lp("/images/lite_client/alert_alert.png")
            }
        }
    ]
}

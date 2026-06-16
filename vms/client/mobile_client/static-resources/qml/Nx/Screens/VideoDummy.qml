// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

VideoPlaceholder
{
    id: control

    signal logInClicked()

    state: ""

    states: [
        State
        {
            name: ""
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: ""
                imageSource: ""
            }
        },
        State
        {
            name: "serverOffline"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Offline")
                imageSource: "image://skin/48x48/Outline/error.svg"
            }
        },
        State
        {
            name: "needsCloudAuthorization"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Information required")
                imageSource: "image://skin/48x48/Outline/lock.svg"
                action: logInAction
            }
        },
        State
        {
            name: "is2FaDisabled"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("2FA disabled")
                imageSource: "image://skin/48x48/Outline/restrict.svg"
            }
        },
        State
        {
            name: "cameraUnauthorized"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Unauthorized")
                imageSource: "image://skin/48x48/Outline/lock.svg"
            }
        },
        State
        {
            name: "cameraOffline"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Offline")
                imageSource: "image://skin/48x48/Outline/error.svg"
            }
        },
        State
        {
            name: "videoLoadingFailed"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Cannot load video")
                imageSource: "image://skin/48x48/Outline/error.svg"
            }
        },
        State
        {
            name: "noVideoStreams"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Cannot play any video stream")
                imageSource: "image://skin/48x48/Outline/error.svg"
            }
        },
        State
        {
            name: "noLicenses"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Not enough licenses")
                imageSource: "image://skin/48x48/Outline/restrict.svg"
            }
        },
        State
        {
            name: "defaultPasswordAlert"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Password required")
                imageSource: "image://skin/48x48/Outline/lock.svg"
            }
        },
        State
        {
            name: "audioOnlyMode"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: ""
                imageSource: "image://skin/48x48/Outline/sound.svg"
            }
        },
        State
        {
            name: "oldFirmwareAlert"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Error
                text: qsTr("Unsupported")
                imageSource: "image://skin/48x48/Outline/restrict.svg"
            }
        },
        State
        {
            name: "tooManyConnections"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: qsTr("Too many connections")
                imageSource: "image://skin/48x48/Outline/restrict.svg"
            }
        },
        State
        {
            name: "cannotDecryptMedia"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: qsTr("Archive encrypted")
                imageSource: "image://skin/48x48/Outline/restrict.svg"
            }
        },
        State
        {
            name: "ioModuleWarning"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: qsTr("I/O interface not supported yet")
                imageSource: "image://skin/48x48/Outline/sound.svg"
            }
        },
        State
        {
            name: "ioModuleAudioPlaying"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: ""
                imageSource: "image://skin/48x48/Outline/sound.svg"
            }
        },
        State
        {
            name: "noLiveStream"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: qsTr("No live stream")
                imageSource: "image://skin/48x48/Outline/restrict.svg"
            }
        },
        State
        {
            name: "noData"
            PropertyChanges
            {
                target: control
                type: VideoPlaceholder.Info
                text: qsTr("No data")
                imageSource: "image://skin/48x48/Outline/restrict.svg"
            }
        }
    ]

    Action
    {
        id: logInAction

        text: qsTr("Log In")
        onTriggered: control.logInClicked()
    }
}

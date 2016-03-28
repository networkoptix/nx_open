import QtQuick 2.4

import com.networkoptix.qml 1.0
import QtMultimedia 5.5

import "../controls"
import "../icons"
import "../items"

Item {
    id: cameraItem

    property alias text: label.text
    property string thumbnail
    property int status
    property string resourceId
    property bool useVideo: false
    property bool paused: false

    signal clicked
    signal pressAndHold
    signal thumbnailRefreshRequested

    QtObject {
        id: d

        property bool hidden: false
        property bool offline: status == QnCameraListModel.Offline || status == QnCameraListModel.NotDefined || status == QnCameraListModel.Unauthorized
        property bool unauthorized: status == QnCameraListModel.Unauthorized

        onHiddenChanged: {
            if (hidden)
                return

            content.x = 0
        }
    }

    Rectangle {
        id: content

        anchors.fill: parent
        color: QnTheme.windowBackground

        Rectangle
        {
            id: thumbnailContainer

            width: parent.width
            height: parent.height - informationRow.height - dp(8)

            color: d.offline ? QnTheme.cameraOfflineBackground : QnTheme.cameraBackground

            Loader {
                id: thumbnailContentLoader

                anchors.centerIn: parent

                sourceComponent: {
                    if (d.offline || d.unauthorized)
                        return thumbnailDummyComponent
                    else if (useVideo)
                        return videoComponent
                    else if (!cameraItem.thumbnail)
                        return thumbnailPreloaderComponent
                    else
                        return thumbnailComponent
                }
            }
        }

        Row {
            id: informationRow

            width: parent.width
            anchors.bottom: parent.bottom
            spacing: dp(8)

            QnStatusIndicator {
                id: statusIndicator
                status: cameraItem.status
                y: dp(4)
            }

            Text {
                id: label
                width: parent.width - x - parent.spacing
                height: dp(24)
                maximumLineCount: 1
                font.pixelSize: sp(16)
                font.weight: d.offline ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                color: d.offline ? QnTheme.cameraOfflineText : QnTheme.cameraText
            }
        }

        Behavior on x { NumberAnimation { duration: 200 } }
    }

    Component {
        id: thumbnailDummyComponent

        Column {
            width: thumbnailContainer.width - dp(16)

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: d.unauthorized ? "image://icon/camera_locked.png" : "image://icon/camera_offline.png"
                width: dp(64)
                height: dp(64)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: dp(32)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                text: d.unauthorized ? qsTr("Authentication\nrequired") : qsTr("Offline")
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                font.pixelSize: sp(14)
                font.weight: Font.Normal
                color: QnTheme.cameraText
            }
        }
    }

    Component {
        id: thumbnailPreloaderComponent

        QnThreeDotPreloader {}
    }

    Component {
        id: thumbnailComponent

        Image {
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

            VideoOutput
            {
                anchors.fill: parent
                source: player
            }

            QnMediaPlayer
            {
                id: player

                resourceId: cameraItem.resourceId
                Component.onCompleted: playLive()
                videoQuality: QnPlayer.Low
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

    QnMaterialSurface {
        id: materialSurface

        anchors.fill: parent

        onClicked: cameraItem.clicked()
        onPressAndHold: cameraItem.pressAndHold()
    }

    Timer {
        id: refreshTimer

        readonly property int initialLoadDelay: 400
        readonly property int reloadDelay: 60 * 1000

        interval: initialLoadDelay
        repeat: true
        running: connectionManager.connectionState == QnConnectionManager.Connected

        onTriggered: {
            interval = reloadDelay
            cameraItem.thumbnailRefreshRequested()
        }

        onRunningChanged: {
            if (!running)
                interval = initialLoadDelay
        }
    }
}

import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"
import "../icons"

Item {
    id: cameraItem

    property alias text: label.text
    property string thumbnail
    property int status

    readonly property alias hidden: d.hidden

    signal clicked
    signal pressAndHold

    height: content.height + dp(8)

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

        width: parent.width
        height: contentColumn.height
        color: QnTheme.windowBackground
        anchors.margins: -dp(8)

        opacity: Math.max(0.0, 1.0 - Math.abs(content.x) / content.width)

        Column {
            id: contentColumn
            spacing: dp(8)

            width: parent.width

            Rectangle {
                id: thumbnailContainer

                width: parent.width
                height: parent.width * 3 / 4
                color: d.offline ? QnTheme.cameraOfflineBackground : QnTheme.cameraBackground

                Loader {
                    id: thumbnailContentLoader

                    anchors.centerIn: parent

                    sourceComponent: {
                        if (d.offline || d.unauthorized)
                            return thumbnailDummyComponent
                        else if (!cameraItem.thumbnail)
                            return thumbnailPreloaderComponent
                        else
                            return thumbnailComponent
                    }
                }
            }

            Row {
                width: parent.width
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

    QnMaterialSurface {
        id: materialSurface

        enabled: !d.hidden

        anchors.fill: content
        anchors.margins: -dp(8)

        onClicked: cameraItem.clicked()
        onPressAndHold: cameraItem.pressAndHold()

        drag.axis: Drag.XAxis
        drag.target: content
        drag.threshold: dp(8)

        QnPropertyChangeSpeedMeasurer
        {
            value: content.x
            active: materialSurface.drag.active
            onActiveChanged:
            {
                if (active)
                    return

                var dx = Math.abs(content.x)

                if ((Math.abs(speed) < dp(800) && dx < content.width * 0.85) || dx < content.width * 0.25)
                {
                    content.x = 0
                    return
                }

                content.x = speed > 0 ? content.width : -content.width
                d.hidden = true
            }
        }
    }

    Binding {
        target: cameraItem
        property: "z"
        value: materialSurface.drag.active ? 5 : 0
    }
}

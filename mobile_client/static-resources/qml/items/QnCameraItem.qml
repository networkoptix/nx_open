import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: cameraItem

    property alias text: label.text
    property alias thumbnail: thumbnail.source
    property int status

    property alias thumbnailWidth: thumbnail.sourceSize.width
    property alias thumbnailHeight: thumbnail.sourceSize.height

    readonly property alias hidden: d.hidden

    signal clicked
    signal pressAndHold

    width: content.width
    height: content.height

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

        width: contentColumn.width
        height: contentColumn.height
        color: QnTheme.windowBackground
        anchors.margins: -dp(8)

        opacity: Math.max(0.0, 1.0 - Math.abs(content.x) / content.width)

        Column {
            id: contentColumn
            spacing: dp(8)

            width: thumbnail.width

            Item {
                id: thumbnailContainer

                width: thumbnailWidth
                height: thumbnailDummy.visible ? thumbnailDummy.height : thumbnail.height

                Rectangle {
                    id: thumbnailDummy
                    width: thumbnailWidth
                    height: thumbnailWidth * 3 / 4
                    border.color: QnTheme.cameraBorder
                    border.width: dp(1)
                    color: "transparent"
                    visible: thumbnail.status != Image.Ready

                    Text {
                        anchors.centerIn: parent
                        text: d.unauthorized ? qsTr("Unauthorized") : qsTr("Offline")
                        font.capitalization: Font.AllUppercase
                        font.pixelSize: sp(18)
                        font.weight: Font.Light
                        color: QnTheme.cameraOfflineText
                    }
                }

                Image {
                    id: thumbnail
                    anchors.fill: parent
                    fillMode: Qt.KeepAspectRatio
                }
            }

            Row {
                width: parent.width
                spacing: dp(8)

                QnStatusIndicator {
                    id: statusIndicator
                    status: cameraItem.status
                    y: dp(2)
                }

                Text {
                    id: label
                    width: parent.width - statusIndicator.width - 2 * anchors.margins
                    maximumLineCount: 2
                    font.pixelSize: sp(15)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap
                    color: d.offline ? QnTheme.cameraOfflineText : QnTheme.cameraText
                }
            }
        }

        Behavior on x { NumberAnimation { duration: 200 } }
    }

    Rectangle {
        id: hiddenDummy

        width: thumbnailWidth
        height: thumbnailWidth * 3 / 4
        border.color: QnTheme.cameraBorder
        border.width: dp(1)
        color: QnTheme.windowBackground

        opacity: 1.0 - Math.min(0.5, content.opacity) * 2

        Column {
            width: parent.width
            anchors.centerIn: parent
            spacing: dp(8)

            Text {
                width: parent.width

                text: qsTr("Camera hidden")

                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: sp(15)
                font.weight: Font.Light
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                color: QnTheme.cameraOfflineText
            }

            QnButton {
                id: undoButton

                anchors.horizontalCenter: parent.horizontalCenter

                flat: true
                text: qsTr("Undo")
                icon: "image://icon/undo.png"

                onClicked: d.hidden = false
            }
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
        drag.onActiveChanged: {
            if (drag.active)
                return

            if (Math.abs(content.x) > content.width / 2) {
                content.x = content.x > 0 ? content.width : -content.width
                d.hidden = true
            } else {
                content.x = 0
            }
        }
        drag.threshold: dp(8)
    }

    z: materialSurface.drag.active ? 5 : 0
}


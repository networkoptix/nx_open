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

    signal clicked
    signal pressAndHold

    width: content.width
    height: content.height

    property bool _offline: status == 0 || status == 4 || status == 1
    property bool _unauthorized: status == 1

    Column {
        id: content
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
                    text: _unauthorized ? qsTr("Unauthorized") : qsTr("Offline")
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
                color: _offline ? QnTheme.cameraOfflineText : QnTheme.cameraText
            }
        }
    }

    QnMaterialSurface {
        onClicked: cameraItem.clicked()
        onPressAndHold: cameraItem.pressAndHold()
    }
}


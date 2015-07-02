import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: cameraItem

    property alias text: label.text
    property alias thumbnail: thumbnail.source
    property int status

    signal clicked
    signal pressAndHold

    width: parent.width
    height: dp(56)

    property bool _offline: status == QnCameraListModel.Offline || status == QnCameraListModel.NotDefined || status == QnCameraListModel.Unauthorized
    property bool _unauthorized: status == QnCameraListModel.Unauthorized

    Item {
        id: thumbnailContainer

        width: dp(80)
        height: parent.height

        Rectangle {
            id: thumbnailDummy
            width: parent.width
            height: parent.height
            border.color: QnTheme.cameraBorder
            border.width: dp(1)
            color: "transparent"
            visible: thumbnail.status != Image.Ready

            Text {
                anchors.centerIn: parent
                text: _unauthorized ? qsTr("Unauthorized") : qsTr("Offline")
                font.capitalization: Font.AllUppercase
                font.pixelSize: sp(10)
                minimumPixelSize: sp(6)
                font.weight: Font.Light
                fontSizeMode: Text.HorizontalFit
                horizontalAlignment: Text.AlignHCenter
                width: parent.width - dp(16)
                color: QnTheme.cameraOfflineText
            }
        }

        Image {
            id: thumbnail
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            fillMode: Qt.KeepAspectRatio
            sourceSize.width: parent.width
            sourceSize.height: parent.height
        }
    }

    Row {
        spacing: dp(8)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: thumbnailContainer.right
        anchors.leftMargin: dp(16)
        anchors.right: parent.right
        height: label.height

        QnStatusIndicator {
            id: statusIndicator
            status: cameraItem.status
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: label
            width: parent.width - statusIndicator.width - 2 * anchors.margins
            font.pixelSize: sp(15)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            color: _offline ? QnTheme.cameraOfflineText : QnTheme.cameraText
        }
    }

    QnMaterialSurface {
        onClicked: cameraItem.clicked()
        onPressAndHold: cameraItem.pressAndHold()
    }
}

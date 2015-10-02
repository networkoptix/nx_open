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

    QtObject {
        id: d

        property bool offline: status == QnCameraListModel.Offline || status == QnCameraListModel.NotDefined || status == QnCameraListModel.Unauthorized
        property bool unauthorized: status == QnCameraListModel.Unauthorized
    }

    Item {
        id: thumbnailContainer

        width: dp(80)
        height: parent.height

        Rectangle {
            id: thumbnailDummy
            width: parent.width
            height: parent.height
            color: d.offline ? QnTheme.cameraOfflineBackground : QnTheme.cameraBackground

        }

        Image {
            anchors.centerIn: parent
            source: d.unauthorized ? "image://icon/camera_locked.png" : "image://icon/camera_offline.png"
            visible: d.offline
            sourceSize.width: dp(40)
            sourceSize.height: dp(40)
        }

        Loader {
            id: preloader
            anchors.centerIn: parent
            source: (!d.offline && thumbnail.status != Image.Ready) ? Qt.resolvedUrl("qrc:///qml/icons/QnThreeDotPreloader.qml") : ""
            scale: 0.5
        }

        Image {
            id: thumbnail
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            fillMode: Qt.KeepAspectRatio
            sourceSize.width: parent.width
            sourceSize.height: parent.height
            visible: !d.offline && status == Image.Ready
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
            color: d.offline ? QnTheme.cameraOfflineText : QnTheme.cameraText
        }
    }

    QnMaterialSurface {
        onClicked: cameraItem.clicked()
        onPressAndHold: cameraItem.pressAndHold()
    }
}

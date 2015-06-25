import QtQuick 2.4
import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: cameraItem

    property alias text: label.text
    property int status

    signal clicked
    signal pressAndHold
    signal showClicked

    width: parent ? parent.width : 0
    height: dp(56)

    property bool _offline: status == 0 || status == 4 || status == 1
    property bool _unauthorized: status == 1

    QnButton {
        id: showButton
        text: qsTr("Show")
        anchors.right: parent.right
        anchors.rightMargin: dp(16)
        anchors.verticalCenter: parent.verticalCenter
        z: 1

        onClicked: cameraItem.showClicked()
    }

    Row {
        spacing: dp(8)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: dp(16)
        anchors.right: showButton.left
        anchors.rightMargin: dp(16)
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

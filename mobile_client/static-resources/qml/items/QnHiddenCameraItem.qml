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

    implicitWidth: parent ? parent.width : 0
    implicitHeight: dp(56)

    readonly property bool offline: status == QnCameraListModel.Offline ||
                                    status == QnCameraListModel.NotDefined ||
                                    status == QnCameraListModel.Unauthorized

    QnMaterialSurface {
        anchors.fill: parent
        onClicked: cameraItem.clicked()
        onPressAndHold: cameraItem.pressAndHold()
    }

    QnStatusIndicator {
        id: statusIndicator

        x: dp(8)
        anchors.verticalCenter: parent.verticalCenter

        status: cameraItem.status
    }

    Text {
        id: label

        height: parent.height
        anchors {
            left: statusIndicator.right
            right: showButton.left
            margins: dp(8)
        }

        verticalAlignment: Text.AlignVCenter
        maximumLineCount: 2
        wrapMode: Text.WordWrap
        font.pixelSize: sp(16)
        font.weight: offline ? Font.DemiBold : Font.Normal
        elide: Text.ElideRight
        color: offline ? QnTheme.cameraOfflineText : QnTheme.cameraText
    }

    QnButton {
        id: showButton

        width: Math.max(implicitWidth, dp(80))
        anchors.right: parent.right
        anchors.rightMargin: dp(4)
        anchors.verticalCenter: parent.verticalCenter

        text: qsTr("Show")
        font.pixelSize: sp(13)
        font.weight: Font.DemiBold
        color: QnTheme.cameraShowButtonBackground

        onClicked: cameraItem.showClicked()
    }
}

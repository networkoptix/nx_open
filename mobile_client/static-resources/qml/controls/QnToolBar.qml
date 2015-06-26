import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: toolbar

    property alias title: label.text

    width: parent.width
    height: dp(56)
    clip: true

    Rectangle {
        id: backgroundRectangle
        anchors.fill: parent
        color: QnTheme.windowBackground

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: dp(1)
            color: QnTheme.listSeparator
        }
    }

    Text {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        x: dp(72)
        font.pixelSize: sp(20)
        font.weight: Font.DemiBold
        color: QnTheme.windowText
    }
}

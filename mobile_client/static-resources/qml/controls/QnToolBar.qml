import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: toolbar

    property alias title: label.text

    property alias backgroundOpacity: backgroundRectangle.opacity

    width: parent.width
    height: dp(56)
    clip: true

    Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

    Image {
        id: backgroundGradient
        anchors.fill: parent
        anchors.topMargin: -dp(24)
        source: "qrc:///images/timeline_gradient.png"
        rotation: 180
    }

    Rectangle {
        id: backgroundRectangle
        anchors.fill: parent
        color: QnTheme.windowBackground

        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

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

import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: toolbar

    property alias title: label.text
    property bool useShadeSize: Qt.platform.os == "android"
    property alias statusBarHeight: statusBarShade.height

    readonly property real fullHeight: height + (useShadeSize ? statusBarHeight : 0)

    property alias backgroundOpacity: backgroundRectangle.opacity

    property var contentItem: contentItem

    width: parent.width
    height: bar.height
    anchors.top: parent.top
    anchors.topMargin: useShadeSize ? statusBarShade.height : 0

    Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }    

    Image {
        id: backgroundGradient
        anchors.fill: parent
        anchors.topMargin: -statusBarShade.height
        source: "qrc:///images/toolbar_gradient.png"
    }

    Rectangle {
        id: statusBarShade
        width: parent.width
        height: getStatusBarHeight()
        anchors.bottom: bar.top
        color: backgroundRectangle.color
        opacity: backgroundRectangle.opacity
    }

    Item {
        id: bar

        clip: true
        width: parent.width
        height: dp(56)

        Rectangle {
            id: backgroundRectangle
            anchors.fill: parent
            color: QnTheme.windowBackground
            Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
        }

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            x: dp(72)
            font.pixelSize: sp(20)
            font.weight: Font.DemiBold
            color: QnTheme.windowText
        }

        Item {
            id: contentItem
            clip: true
            anchors.fill: parent
        }

        Rectangle {
            id: outline
            anchors.bottom: backgroundRectangle.bottom
            width: parent.width
            height: dp(1)
            color: QnTheme.listSeparator
            opacity: backgroundRectangle.opacity
        }
    }
}

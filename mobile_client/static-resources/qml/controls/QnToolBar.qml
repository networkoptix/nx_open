import QtQuick 2.4

Item {
    id: toolbar

    property alias color: backgroundRectangle.color
    property alias textColor: label.color

    property Component leftComponent
    property Component rightComponent
    property alias label: label.text

    width: parent.width
    height: dp(56)

    Rectangle {
        id: backgroundRectangle
        anchors.fill: parent
        color: "transparent"
    }

    Loader {
        id: leftLoader
        sourceComponent: leftComponent
        anchors.verticalCenter: parent.verticalCenter
        x: dp(16)
    }

    Loader {
        id: rightLoader
        sourceComponent: rightComponent
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: dp(16)
    }

    Text {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        x: dp(72)
        font.pixelSize: sp(20)
    }
}

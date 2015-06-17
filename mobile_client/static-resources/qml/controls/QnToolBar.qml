import QtQuick 2.4

Item {
    id: toolbar

    property alias color: backgroundRectangle.color
    property alias textColor: label.color

    property alias leftComponent: leftLoader.sourceComponent
    property alias rightComponent: rightLoader.sourceComponent
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
        anchors.verticalCenter: parent.verticalCenter
        x: dp(10)
    }

    Loader {
        id: rightLoader
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: dp(10)
    }

    Text {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        x: dp(72)
        font.pixelSize: sp(20)
        font.weight: Font.DemiBold
    }
}

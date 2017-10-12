import QtQuick 2.6
import Nx 1.0

Item
{
    property alias titleText: title.text

    property alias backgroundColor: background.color
    property alias backgroundOpacity: background.opacity
    property alias leftContentOpacity: leftContent.opacity
    property alias rightContentOpacity: rightContent.opacity

    implicitWidth: parent ? parent.width : 120
    implicitHeight: 26

    Rectangle
    {
        id: background
        anchors.fill: parent
        color: ColorTheme.colors.dark1
    }

    Row
    {
        id: leftContent
        height: parent.height
    }

    Text
    {
        id: title

        anchors.left: leftContent.right
        anchors.right: rightContent.left
        height: parent.height

        verticalAlignment: Text.AlignVCenter
        leftPadding: 3
        rightPadding: 3

        font.pixelSize: 15
        font.weight: Font.Bold

        color: ColorTheme.brightText
    }

    Row
    {
        id: rightContent
        height: parent.height
        anchors.right: parent.right
    }
}

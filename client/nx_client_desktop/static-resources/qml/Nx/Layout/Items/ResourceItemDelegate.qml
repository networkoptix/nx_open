import QtQuick 2.6
import Nx 1.0

Item
{
    property var modelData

    property bool rotationAllowed: false

    readonly property alias titleBar: titleBar

    readonly property real parentRotation: parent ? parent.rotation : 0

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.colors.dark5
    }

    Item
    {
        id: ui

        anchors.centerIn: parent

        rotation: -Math.round(parentRotation / 90) * 90

        width: rotation % 180 === 0 ? parent.width : parent.height
        height: rotation % 180 === 0 ? parent.height : parent.width

        TitleBar
        {
            id: titleBar
            titleText: modelData.resource ? modelData.resource.name : ""
        }
    }
}

import QtQuick 2.6
import Nx 1.0

Item
{
    id: decoration

    property color color: ColorTheme.base12

    readonly property real extraTopSize: 5
    readonly property real extraBottomSize: 21
    readonly property real extraLeftSize: 5
    readonly property real extraRightSize: 5

    Rectangle
    {
        color: decoration.color
        anchors.fill: parent
        anchors.margins: -5
        radius: 5

        Rectangle
        {
            color: ColorTheme.base5
            anchors.fill: parent
            anchors.margins: 3
        }

        Rectangle
        {
            id: standTop

            color: decoration.color
            width: 64
            height: 10
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom
        }

        Rectangle
        {
            color: decoration.color
            width: 120
            height: 6
            radius: height / 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: standTop.bottom
        }
    }
}

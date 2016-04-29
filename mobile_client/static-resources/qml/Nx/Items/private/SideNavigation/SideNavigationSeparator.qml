import QtQuick 2.0
import Nx 1.0

Item
{
    implicitWidth: parent ? parent.width : 200
    implicitHeight: 32

    Rectangle
    {
        width: parent.width - 32
        height: 1
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        color: ColorTheme.base10
    }
}

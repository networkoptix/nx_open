import QtQuick 2.6
import Nx 1.0

Item
{
    implicitWidth: parent ? parent.width : 100
    implicitHeight: 8

    Rectangle
    {
        width: parent.width - 32
        height: 1
        anchors.centerIn: parent

        color: ColorTheme.contrast6
    }
}

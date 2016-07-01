import QtQuick 2.0
import Nx 1.0

Item
{
    implicitWidth: parent ? parent.width : 200
    implicitHeight: 8

    Rectangle
    {
        width: parent.width
        anchors.bottom: parent.top
        height: 4
        color: ColorTheme.transparent(ColorTheme.base4, 0.2)
    }

    Rectangle
    {
        width: parent.width
        height: 1
        color: ColorTheme.base9
    }
}

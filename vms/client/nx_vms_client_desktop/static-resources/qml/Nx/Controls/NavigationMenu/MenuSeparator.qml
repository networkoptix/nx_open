import QtQuick 2.0
import Nx 1.0

Item
{
    implicitHeight: 16
    implicitWidth: parent.width

    Rectangle
    {
        width: parent.width
        height: 1
        y: 7
        color: ColorTheme.shadow
    }

    Rectangle
    {
        width: parent.width
        height: 1
        y: 8
        color: ColorTheme.mid
    }
}

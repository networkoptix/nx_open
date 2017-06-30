import QtQuick 2.6
import Nx 1.0

Item
{
    property int padding: 16
    implicitWidth: parent ? parent.width : 100
    implicitHeight: 8

    Rectangle
    {
        width: parent.width - 2 * padding
        height: 1
        anchors.centerIn: parent

        color: ColorTheme.contrast6
    }
}

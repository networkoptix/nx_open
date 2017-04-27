import QtQuick 2.6
import Nx 1.0

MouseArea
{
    id: control

    property int spotRadius: width / 2

    Rectangle
    {
        id: spotItem

        width: control.spotRadius * 2
        height: width
        radius: control.spotRadius
        visible: control.pressed
        anchors.centerIn: parent

        color: ColorTheme.transparent(ColorTheme.contrast1, 0.2)
    }
}

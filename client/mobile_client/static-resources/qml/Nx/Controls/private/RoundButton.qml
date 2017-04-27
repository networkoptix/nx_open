import QtQuick 2.6

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

        color: "grey" // TODO change me
    }
}

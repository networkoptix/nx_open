import QtQuick 2.0

Item
{
    id: marker

    property bool hovered: false
    property bool ghost: false

    property color color: "black"
    property color ghostColor: color
    property color borderColor: "white"

    Rectangle
    {
        width: hovered ? 12 : 8
        height: width

        x: -width / 2
        y: -height / 2

        color: ghost ? ghostColor : marker.color
        border.color: ghost ? "transparent" : borderColor
        border.width: 1

        opacity: ghost ? 0.7 : 1.0
    }
}

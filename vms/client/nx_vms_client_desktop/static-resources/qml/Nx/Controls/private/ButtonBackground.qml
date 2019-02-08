import QtQuick 2.11
import Nx 1.0

Rectangle
{
    property bool hovered: false
    property bool pressed: false
    property bool flat: false

    property color backgroundColor: ColorTheme.button
    property color hoveredColor: ColorTheme.lighter(backgroundColor, 1)
    property color pressedColor: ColorTheme.darker(backgroundColor, 1)
    property color outlineColor: ColorTheme.darker(backgroundColor, 2)

    color: (hovered && !pressed)
        ? hoveredColor
        : pressed
            ? pressedColor
            : backgroundColor

    radius: 2

    Rectangle
    {
        width: parent.width
        height: 1
        color: ColorTheme.darker(outlineColor, 1)
        visible: !flat && pressed
    }

    Rectangle
    {
        width: parent.width - 2
        height: 1
        x: 1
        y: parent.height - 1
        color: ColorTheme.lighter(outlineColor, hovered ? 1 : 0)
        visible: !flat && !pressed
    }
}

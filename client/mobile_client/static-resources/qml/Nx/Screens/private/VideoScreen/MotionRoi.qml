import QtQuick 2.6
import Nx 1.0

// Test control for now. Represents simple ROI
Rectangle
{
    id: item

    property bool drawing: false
    property color baseColor: ColorTheme.contrast1

    border.width: 1
    border.color: baseColor

    color: singlePoint ? "red" : "transparent"

    readonly property bool singlePoint: topLeft.minus(bottomRight).length() < 5
    property vector2d topLeft
    property vector2d bottomRight

    x: d.getCoordinate(topLeft.x, bottomRight.x, width)
    y: d.getCoordinate(topLeft.y, bottomRight.y, height)

    width: d.getLength(topLeft.x, bottomRight.x)
    height: d.getLength(topLeft.y, bottomRight.y)

    Rectangle
    {
        x: -width / 2
        y: -height / 2
        width: radius * 2
        height: width
        visible: item.drawing
        color: item.baseColor
        radius: 1.5
    }

    Rectangle
    {
        x: item.width - width / 2
        y: item.height - height / 2
        width: radius * 2
        height: width
        visible: item.drawing
        color: item.baseColor
        radius: 1.5
    }

    Object
    {
        id: d

        function getCoordinate(firstValue, secondValue, length)
        {
            return singlePoint ? firstValue - length / 2 : Math.min(firstValue, secondValue)
        }

        function getLength(firstValue, secondValue)
        {
            return singlePoint ? 10 : Math.abs(secondValue - firstValue)
        }
    }
}

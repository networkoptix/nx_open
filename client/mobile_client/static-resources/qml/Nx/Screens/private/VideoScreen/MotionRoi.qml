import QtQuick 2.6
import Nx 1.0

// Test control for now. Represents simple ROI
Rectangle
{
    property color baseColor: ColorTheme.contrast1

    border.width: 1
    border.color: baseColor

    color: singlePoint ? "red" : "transparent"

    readonly property bool singlePoint: topLeft.minus(bottomRight).length() < 5
    property vector2d topLeft
    property vector2d bottomRight

    x: getCoordinate(topLeft.x, bottomRight.x, width)
    y: getCoordinate(topLeft.y, bottomRight.y, height)

    width: getLength(topLeft.x, bottomRight.x)
    height: getLength(topLeft.y, bottomRight.y)

    function getCoordinate(firstValue, secondValue, length)
    {
        return singlePoint ? firstValue - length / 2 : Math.min(firstValue, secondValue)
    }

    function getLength(firstValue, secondValue)
    {
        return singlePoint ? 10 : Math.abs(secondValue - firstValue)
    }
}

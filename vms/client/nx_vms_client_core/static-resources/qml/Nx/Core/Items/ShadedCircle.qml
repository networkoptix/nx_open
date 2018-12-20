import QtQuick 2.6

Circle
{
    id: shade

    property alias circleRadius: circle.radius
    property alias circleColor: circle.color
    property color circleBorderColor: circleColor
    property color shadowColor
    property int shadowRadius: 1

    border.width: shadowRadius
    border.color: shadowColor
    radius: circleRadius + shadowRadius
    color: "transparent"

    Circle
    {
        id: circle

        anchors.centerIn: parent
        border.color: shade.circleBorderColor
    }
}

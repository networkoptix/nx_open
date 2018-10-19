import QtQuick 2.6

Circle
{
    id: shade

    property alias circleColor: circle.color
    property color circleBorderColor: circleColor
    property alias shadowColor: shade.color

    Circle
    {
        id: circle

        anchors.centerIn: parent
        radius: parent.radius - 1
        border.color: shade.circleBorderColor
    }
}

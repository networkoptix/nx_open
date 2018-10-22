import QtQuick 2.6

Rectangle
{
    id: item
    property point centerPoint: Qt.point(0, 0)
    property color circleColor: "white"
    property alias innerColor: item.color

    color: "transparent"
    border.width: radius
    border.color: circleColor

    x: centerPoint.x - width / 2
    y: centerPoint.y - height / 2
    width: radius * 2
    height: width
}

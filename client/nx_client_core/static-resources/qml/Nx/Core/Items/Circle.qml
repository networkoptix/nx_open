import QtQuick 2.6

Rectangle
{
    property point centerPoint: Qt.point(0, 0)
    x: centerPoint.x - width / 2
    y: centerPoint.y - height / 2
    width: radius * 2
    height: width
}

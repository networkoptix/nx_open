import QtQuick 2.0
import Nx.Controls 1.0

Item
{
    property color color: "black"

    property string hint: ""
    property int hintStyle: Banner.Style.Info

    width: parent.width
    height: parent.height

    function relativePoint(x, y)
    {
        return Qt.point(relativeX(point.x), relativeY(point.y))
    }

    function absolutePoint(point)
    {
        return Qt.point(absoluteX(point.x), absoluteY(point.y))
    }

    function relativeX(x)
    {
        return x / width
    }
    function relativeY(y)
    {
        return y / height
    }
    function absoluteX(x)
    {
        return x * width
    }
    function absoluteY(y)
    {
        return y * height
    }
}

import QtQuick 2.6
import Nx 1.0

Item
{
    property real desiredWidth: implicitWidth
    property real minimumWidth: 0
    property real maximumWidth: Infinity

    property real desiredHeight: implicitHeight
    property real minimumHeight: 0
    property real maximumHeight: Infinity

    readonly property real actualWidth: width * scale
    readonly property real actualHeight: height * scale

    width: MathUtils.bound(minimumWidth, desiredWidth, maximumWidth)
    height: MathUtils.bound(minimumHeight, desiredHeight, maximumHeight)
    scale: Math.min(
        1,
        desiredWidth > 0 ? desiredWidth / width : Infinity,
        desiredHeight > 0 ? desiredHeight / height : Infinity)
}

import QtQuick 2.0

import Nx 1.0

Item
{
    id: control

    property real revertRotationAngle: 0

    x: d.swapWidthAndHeight ? (height - width ) / 2 : 0
    y: d.swapWidthAndHeight ? (width - height) / 2 : 0
    width: d.swapWidthAndHeight ? parent.height : parent.width
    height: d.swapWidthAndHeight ? parent.width : parent.height
    rotation: -revertRotationAngle

    Object
    {
        id: d

        readonly property bool swapWidthAndHeight:
            control.revertRotationAngle == 90
            || control.revertRotationAngle == 270
    }
}


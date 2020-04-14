import QtQuick 2.0

import Nx 1.0

Item
{
    id: control

    property real revertRotationAngle: 0

    x: d.swapWidthandHeight ? (height - width ) / 2 : 0
    y: d.swapWidthandHeight ? (width - height) / 2 : 0
    width: d.swapWidthandHeight ? parent.height : parent.width
    height: d.swapWidthandHeight ? parent.width : parent.height
    rotation: -revertRotationAngle

    Object
    {
        id: d

        readonly property bool swapWidthandHeight:
            control.revertRotationAngle == 90
            || control.revertRotationAngle == 270
    }
}


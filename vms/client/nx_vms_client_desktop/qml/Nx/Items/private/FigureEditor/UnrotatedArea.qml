// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

Item
{
    id: control

    property real revertRotationAngle: 0

    x: d.swapWidthAndHeight ? (height - width ) / 2 : 0
    y: d.swapWidthAndHeight ? (width - height) / 2 : 0
    width: d.swapWidthAndHeight ? parent.height : parent.width
    height: d.swapWidthAndHeight ? parent.width : parent.height
    rotation: -revertRotationAngle

    QtObject
    {
        id: d

        readonly property bool swapWidthAndHeight:
            control.revertRotationAngle == 90
            || control.revertRotationAngle == 270
    }
}

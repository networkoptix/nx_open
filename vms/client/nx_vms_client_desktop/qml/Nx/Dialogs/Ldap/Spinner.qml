// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

BusyIndicator
{
    id: control

    visible: running

    contentItem: Item
    {
        width: 20
        height: 20

        Image
        {
            id: image

            anchors.centerIn: parent

            source: "image://skin/20x20/Outline/loading.svg?primary=light16"
            sourceSize: Qt.size(20, 20)

            transformOrigin: Item.Center
            rotation: 0

            SequentialAnimation on rotation
            {
                id: rotationAnimation

                running: control.running
                loops: Animation.Infinite

                RotationAnimation
                {
                    from: 0
                    to: 360
                    duration: 1200
                    direction: RotationAnimation.Clockwise
                }
            }
        }
    }
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

Item
{
    id: item

    property bool active: false
    property int swingDurationMs: 800

    visible: active

    ColoredImage
    {
        id: image

        sourcePath: "image://skin/events/tabs/notifications.svg"
        sourceSize: Qt.size(20, 20)
        primaryColor: "green_l2"
        name: "CallNotificationIcon"

        anchors.centerIn: item

        transform: Rotation
        {
            id: rotation

            origin.x: image.width / 2
            origin.y: 6
        }
    }

    SequentialAnimation
    {
        id: animation

        running: item.active

        RotationAnimation
        {
            id: initialHalfSwing

            target: rotation
            direction: RotationAnimation.Clockwise
            duration: swingDurationMs / 2
            easing.type: Easing.OutCubic
            from: 0
            to: 45
        }

        SequentialAnimation
        {
            id: loopedFullSwings

            loops: Animation.Infinite

            RotationAnimation
            {
                target: rotation
                direction: RotationAnimation.Counterclockwise
                duration: swingDurationMs
                easing.type: Easing.InOutCubic
                to: -45
            }

            RotationAnimation
            {
                target: rotation
                direction: RotationAnimation.Clockwise
                duration: swingDurationMs
                easing.type: Easing.InOutCubic
                to: 45
            }
        }
    }
}

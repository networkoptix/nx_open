// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    id: controller

    property real skeletonWidth: 900
    property real fillerWidth: 180

    property real speed: 600 //< Pixels/second

    property alias fillerPosition: animation.value

    SequentialAnimation
    {
        id: animation

        property real value: -controller.fillerWidth

        loops: Animation.Infinite
        running: true

        NumberAnimation
        {
            id: moveAnimation

            target: animation
            property: "value"
            from: -controller.fillerWidth
            to: controller.skeletonWidth
            duration: 1000 * (controller.skeletonWidth + controller.fillerWidth) / controller.speed
            easing.type: Easing.Linear
        }

        PauseAnimation
        {
            duration: moveAnimation.duration * 0.8
        }
    }
}

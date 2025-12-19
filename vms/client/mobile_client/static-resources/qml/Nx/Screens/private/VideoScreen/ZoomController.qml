// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    id: controller

    enum ZoomingState { Stopped, ZoomingIn, ZoomingOut }
    property int zoomingState: ZoomController.Stopped

    // Zoom factors per second.
    property real initialSpeed: 10.0
    property real maximumSpeed: 100.0

    property real speedIncreasePeriodMs: 2000
    property int speedIncreaseEasingType: Easing.InQuad

    signal zoomStep(real factor)

    NumberAnimation
    {
        id: timeAnimation

        readonly property int kLoopSeconds: 10

        target: d
        property: "currentSeconds"
        from: 0.0
        to: kLoopSeconds
        duration: kLoopSeconds * 1000
        easing.type: Easing.Linear
        loops: Animation.Infinite

        running: controller.zoomingState === ZoomController.ZoomingIn
            || controller.zoomingState === ZoomController.ZoomingOut

        onStarted:
            speedAnimation.restart()

        onStopped:
        {
            d.previousSeconds = 0.0
            speedAnimation.stop()
        }
    }

    NumberAnimation
    {
        id: speedAnimation

        target: d
        property: "currentSpeed"
        from: controller.initialSpeed
        to: controller.maximumSpeed

        easing.type: controller.speedIncreaseEasingType
        duration: speedIncreasePeriodMs
    }

    NxObject
    {
        id: d

        property real currentSpeed: 1.0
        property real currentSeconds: 0.0
        property real previousSeconds: 0.0

        onCurrentSecondsChanged:
        {
            let elapsed = currentSeconds - previousSeconds
            if (Math.abs(elapsed) < 0.001)
                return

            while (elapsed < 0)
                elapsed += timeAnimation.kLoopSeconds

            const factor = Math.pow(currentSpeed, -elapsed)
            previousSeconds = currentSeconds

            controller.zoomStep(
                controller.zoomingState === ZoomController.ZoomingIn ? factor : (1.0 / factor))
        }
    }
}

import QtQuick 2.6
import Nx 1.0

Object
{
    id: controller

    property bool enabled: true
    property FisheyeInteractor interactor: null
    property PinchArea pinchArea: null
    property MouseArea mouseArea: null

    readonly property bool enabledAnimation:
        mouseArea
        && pinchArea
        && pinchAreaHandler.zoomStarted
        && !mouseArea.draggingStarted
        && !kineticAnimator.inertia

    onMouseAreaChanged:
    {
        if (mouseArea)
            mouseArea.drag.threshold = 10
    }

    Object
    {
        id: d

        KineticPositionAnimator
        {
            id: kineticAnimator

            onPositionChanged:
            {
                const kSensitivity = 100.0
                var normalization = kSensitivity / mouseAreaHandler.pixelRadius
                var dx = position.x - initialPosition.x
                var dy = position.y - initialPosition.y
                interactor.updateRotation(dy * normalization, dx * normalization)
            }
        }

        function updateZoom(x, y, scale)
        {
            interactor.updateZoom(x, y, Math.log(scale) / Math.LN2, false)
        }
    }

    Object
    {
        id: pinchAreaHandler

        property bool zoomStarted: false

        Connections
        {
            target: pinchArea
            enabled: target && controller.enabled
            onPinchStarted:
            {
                interactor.startZoom(pinch.startCenter.x, pinch.startCenter.y)
                kineticAnimator.interrupt()
                pinchAreaHandler.zoomStarted = true
            }

            onPinchUpdated:
            {
                d.updateZoom(pinch.center.x, pinch.center.y, pinch.scale)
            }

            onPinchFinished:
            {
                d.updateZoom(pinch.center.x, pinch.center.y, pinch.scale)
                pinchAreaHandler.zoomStarted = false
            }
        }
    }

    Object
    {
        id: mouseAreaHandler

        property bool draggingStarted: false
        property real pressX: 0
        property real pressY: 0
        property point lastClickPosition: Qt.point(0, 0)

        readonly property real pixelRadius: mouseArea
            ? Math.min(mouseArea.width, mouseArea.height) / 2.0
            : 1

        Connections
        {
            target: controller.mouseArea
            enabled: target && controller.enabled

            onPressed:
            {
                mouseAreaHandler.pressX = mouse.x
                mouseAreaHandler.pressY = mouse.y
                kineticAnimator.interrupt()
            }

            onDoubleClicked:
            {
                var distance = Qt.vector2d(
                    mouseAreaHandler.lastClickPosition.x - mouse.x,
                    mouseAreaHandler.lastClickPosition.y - mouse.y).length()

                if (distance > mouseArea.drag.threshold)
                    return

                interactor.enableScaleAnimation = true

                const kPowerThreshold = 0.8
                if (interactor.scalePower > kPowerThreshold)
                {
                    interactor.scalePower = 0.0
                }
                else
                {
                    interactor.centerAtPixel(mouse.x, mouse.y)
                    interactor.scalePower = 1.0
                }
            }

            onReleased:
            {
                if (!mouseAreaHandler.draggingStarted)
                    return

                kineticAnimator.finishMeasurement(mouse.x, mouse.y)
                mouseAreaHandler.draggingStarted = false
            }

            onClicked:
            {
                mouseAreaHandler.lastClickPosition = Qt.point(mouse.x, mouse.y)
            }

            onPositionChanged:
            {
                if (mouse.buttons & Qt.LeftButton)
                {
                    if (mouseAreaHandler.draggingStarted)
                    {
                        kineticAnimator.updateMeasurement(mouse.x, mouse.y)
                    }
                    else
                    {
                        mouseAreaHandler.draggingStarted =
                            Math.abs(mouse.x - mouseAreaHandler.pressX) > mouseArea.drag.threshold
                            || Math.abs(mouse.y - mouseAreaHandler.pressY) > mouseArea.drag.threshold

                        if (mouseAreaHandler.draggingStarted)
                        {
                            kineticAnimator.startMeasurement(mouse.x, mouse.y)
                            interactor.startRotation()
                        }
                    }
                }
            }

            onWheel:
            {
                var kSensitivity = 100.0
                var deltaPower = wheel.angleDelta.y * kSensitivity / 1.0e5

                interactor.startZoom(wheel.x, wheel.y)
                interactor.updateZoom(wheel.x, wheel.y, deltaPower, true)

                if (mouseAreaHandler.draggingStarted)
                    interactor.startRotation()
            }
        }

    }
}

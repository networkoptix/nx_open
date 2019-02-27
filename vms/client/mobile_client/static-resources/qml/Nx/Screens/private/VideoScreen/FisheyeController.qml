import QtQuick 2.6
import Nx 1.0

Object
{
    id: controller

    property bool enabled: true
    property FisheyeInteractor interactor: null
    property PinchArea pinchArea: null
    property MouseArea mouseArea: null

    readonly property bool enableAnimation:
        mouseArea
        && pinchArea
        && pinchAreaHandler.zoomStarted
        && !mouseArea.draggingStarted
        && !kineticAnimator.inertia

    signal clicked()

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
            interactor.updateZoom(x, y, Math.log(scale) / Math.LN2)
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

        // TODO: #ynikitenokov Refactor code, make generic mousearea handler
        // that filters (double)click events.
        Timer
        {
            id: delayedClickTimer

            interval: 300
            onTriggered: controller.clicked()
        }

        Timer
        {
            id: clickFilterTimer
            interval: 400
        }

        Timer
        {
            id: doubleClickFilterTimer
            interval: 300
        }

        Connections
        {
            target: controller.mouseArea
            enabled: target && controller.enabled

            onPressed:
            {
                mouseAreaHandler.pressX = mouse.x
                mouseAreaHandler.pressY = mouse.y
                kineticAnimator.interrupt()

                clickFilterTimer.restart()
            }

            onCanceled: delayedClickTimer.stop()

            onDoubleClicked:
            {
                delayedClickTimer.stop()
                doubleClickFilterTimer.restart()

                if (!Utils.nearPositions(mouseAreaHandler.lastClickPosition,
                    Qt.point(mouse.x, mouse.y), mouseArea.drag.threshold))
                {
                    return
                }

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
                if (clickFilterTimer.running && !doubleClickFilterTimer.running)
                    delayedClickTimer.restart()
                else
                    delayedClickTimer.stop()

                if (!mouseAreaHandler.draggingStarted)
                    return

                if (doubleClickFilterTimer.running)
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
                interactor.updateZoom(wheel.x, wheel.y, deltaPower)

                if (mouseAreaHandler.draggingStarted)
                    interactor.startRotation()
            }
        }

    }
}

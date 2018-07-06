import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Media 1.0

Item
{
    id: content

    property alias mediaPlayer: video.player
    property MediaResourceHelper resourceHelper: null

    property real videoCenterHeightOffsetFactor: 0.0

    readonly property var viewMode: resourceHelper
        ? resourceHelper.fisheyeParams.viewMode
        : MediaDewarpingParams.Horizontal

    signal clicked()

    function clear()
    {
        video.clear()
    }

    VideoOutput
    {
        id: video
    }

    FisheyeShaderEffect
    {
        id: fisheyeShader

        sourceItem: video
        anchors.fill: content

        fieldOffset: resourceHelper
            ? Qt.vector2d(0.5 - resourceHelper.fisheyeParams.xCenter, 0.5 - resourceHelper.fisheyeParams.yCenter)
            : Qt.vector2d(0.0, 0.0)

        fieldStretch: resourceHelper ? resourceHelper.fisheyeParams.hStretch : 1.0

        fieldRadius: resourceHelper ? resourceHelper.fisheyeParams.radius : 0.0

        fieldRotation: resourceHelper
            ? (resourceHelper.fisheyeParams.fovRot + resourceHelper.customRotation)
            : 0.0

        viewRotationMatrix: interactor.currentRotationMatrix

        viewScale: interactor.currentScale

        viewShift: width > height
            ? Qt.vector2d(0, 0)
            : Qt.vector2d(0, -(1.0 - width / height) / 2 * videoCenterHeightOffsetFactor)
    }

    Object
    {
        id: interactor

        readonly property real currentScale: Math.pow(2.0, animatedScalePower)

        readonly property matrix4x4 currentRotationMatrix:
        {
            switch (viewMode)
            {
                case MediaDewarpingParams.VerticalUp:
                    return Utils3D.rotationZ(Utils3D.radians(currentRotation.y)).times(
                        Utils3D.rotationX(Utils3D.radians(currentRotation.x)))

                case MediaDewarpingParams.VerticalDown:
                    return Utils3D.rotationZ(-Utils3D.radians(currentRotation.y)).times(
                        Utils3D.rotationX(Utils3D.radians(currentRotation.x)))

                default:
                    return Utils3D.rotationY(Utils3D.radians(currentRotation.y)).times(
                        Utils3D.rotationX(Utils3D.radians(currentRotation.x)))
            }
        }

        /* Interactor implementation: */

        property vector2d currentRotation:
        {
            var limitByEdge = (180.0 - fisheyeShader.fov) / 2.0
            var limitByCenter = Math.min(fisheyeShader.fov / 2.0, limitByEdge)

            switch (viewMode)
            {
                case MediaDewarpingParams.VerticalUp:
                    return Qt.vector2d(Math.max(-limitByEdge, Math.min(-limitByCenter, unboundedRotation.x)),
                        normalizedAngle(unboundedRotation.y))

                case MediaDewarpingParams.VerticalDown:
                    return Qt.vector2d(Math.max(limitByCenter, Math.min(limitByEdge, unboundedRotation.x)),
                        normalizedAngle(unboundedRotation.y))

                default:
                    return Qt.vector2d(Math.max(-limitByEdge, Math.min(limitByEdge, unboundedRotation.x)),
                        Math.max(-limitByEdge, Math.min(limitByEdge, unboundedRotation.y)))
            }
        }

        property vector2d unboundedRotation: Qt.vector2d(0.0, 0.0)

        property vector2d previousRotation

        property real scalePower: 0.0
        property real animatedScalePower: scalePower

        Behavior on animatedScalePower
        {
            id: scalePowerAnimationBehavior
            NumberAnimation { duration: 150 }
        }

        function startRotation()
        {
            previousRotation = currentRotation
        }

        function updateRotation(aroundX, aroundY) // angle deltas since start, in degrees
        {
            var rotationFactor = fisheyeShader.fov / 180.0

            unboundedRotation = previousRotation.plus(Qt.vector2d(
                aroundX * rotationFactor,
                aroundY * rotationFactor))
        }

        function scaleBy(deltaPower, animated)
        {
            scalePowerAnimationBehavior.enabled = animated
            scalePower = Math.min(4.0, Math.max(0.0, scalePower + deltaPower))
            unboundedRotation = currentRotation
        }

        function normalizedAngle(degrees) // brings angle to [-180, 180] range
        {
            var angle = degrees % 360
            if (angle < -180)
                return angle + 360
            else if (angle > 180)
                return angle - 360
            else
                return angle
        }
    }

    PinchArea
    {
        id: pinchArea

        anchors.fill: parent

        property real startScalePower

        onPinchStarted:
        {
            startScalePower = interactor.scalePower
        }

        onPinchUpdated: updateScale(pinch.scale)

        onPinchFinished: updateScale(pinch.scale)

        function updateScale(scale)
        {
            interactor.scalePower = startScalePower
            interactor.scaleBy(Math.log(scale) / Math.LN2, false)
        }

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent

            drag.threshold: 10

            property bool draggingStarted
            property real pressX
            property real pressY

            readonly property real pixelRadius: Math.min(width, height) / 2.0
            readonly property vector2d pixelCenter: Qt.vector2d(width, height).times(0.5)

            KineticAnimation
            {
                id: kinetics

                deceleration: 0.005

                property point startPosition

                onPositionChanged:
                {
                    const kSensitivity = 100.0
                    var normalization = kSensitivity / mouseArea.pixelRadius
                    var dx = position.x - startPosition.x
                    var dy = position.y - startPosition.y
                    interactor.updateRotation(dy * normalization, dx * normalization)
                }

                onMeasurementStarted:
                {
                    startPosition = position
                    interactor.startRotation()
                }
            }

            onPressed:
            {
                pressX = mouse.x
                pressY = mouse.y
            }

            onReleased:
            {
                if (draggingStarted)
                {
                    kinetics.finishMeasurement(Qt.point(mouse.x, mouse.y))
                    draggingStarted = false
                }
                else
                {
                    content.clicked()
                }
            }

            onPositionChanged:
            {
                if (mouse.buttons & Qt.LeftButton)
                {
                    if (draggingStarted)
                    {
                        kinetics.continueMeasurement(Qt.point(mouse.x, mouse.y))
                    }
                    else
                    {
                        draggingStarted = Math.abs(mouse.x - pressX) > drag.threshold
                            || Math.abs(mouse.y - pressY) > drag.threshold

                        if (draggingStarted)
                            kinetics.startMeasurement(Qt.point(mouse.x, mouse.y))
                    }
                }
            }

            onWheel:
            {
                if (draggingStarted)
                    kinetics.startMeasurement(Qt.point(mouse.x, mouse.y))

                const kSensitivity = 100.0
                interactor.scaleBy(wheel.angleDelta.y * kSensitivity / 1.0e5, true)

                if (draggingStarted)
                    interactor.startRotation()
            }
        }
    }
}

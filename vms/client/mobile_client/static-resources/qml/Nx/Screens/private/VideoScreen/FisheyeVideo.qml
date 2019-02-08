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

    readonly property rect videoRect: Qt.rect(video.x, video.y, video.width, video.height)

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

        viewRotationMatrix: interactor.animatedRotationMatrix

        viewScale: interactor.animatedScale

        viewShift: width > height
            ? Qt.vector2d(0, 0)
            : Qt.vector2d(0, -(1.0 - width / height) / 2 * videoCenterHeightOffsetFactor)
    }

    Object
    {
        id: interactor

        // Queryables.

        readonly property real animatedScale: scaleByPower(animatedScalePower)
        readonly property real currentScale: scaleByPower(scalePower)

        readonly property vector2d animatedRotation: constrainRotation(
            Qt.vector2d(animatedRotationX, animatedRotationY), animatedScale)

        readonly property vector2d currentRotation: constrainRotation(
            Qt.vector2d(unconstrainedRotation.x, unconstrainedRotation.y), currentScale)

        readonly property matrix4x4 animatedRotationMatrix: rotationMatrix(animatedRotation)
        readonly property matrix4x4 currentRotationMatrix: rotationMatrix(currentRotation)

        // Interface functions.

        function startRotation()
        {
            previousRotation = Qt.vector2d(animatedRotation.x, animatedRotation.y) //< Deep copy.
        }

        function updateRotation(aroundX, aroundY) //< Angle deltas since start, in degrees.
        {
            var rotationFactor = fisheyeShader.fov(animatedScale) / 180.0
            unconstrainedRotation = Qt.vector2d(
                previousRotation.x + aroundX * rotationFactor,
                previousRotation.y + aroundY * rotationFactor)
        }

        function updateScaleCenter(x, y)
        {
            scaleCenter = currentRotationMatrix.times(fisheyeShader.unproject(
                fisheyeShader.pixelToProjection(x, y, currentScale)))
        }

        function startZoom(x, y)
        {
            previousScalePower = scalePower
            updateScaleCenter(x, y)
        }

        function updateZoom(x, y, deltaPower, animated)
        {
            scalePowerAnimationBehavior.enabled = animated
            scalePower = Math.min(4.0, Math.max(0.0, previousScalePower + deltaPower))

            if (viewMode != MediaDewarpingParams.Horizontal && fisheyeShader.fov(currentScale) >= 90.0)
            {
                updateScaleCenter(x, y)
                return //< No rotations if we have no freedom around axis X.
            }

            var relocatedCenter =
                fisheyeShader.unproject(fisheyeShader.pixelToProjection(x, y, currentScale))

            unconstrainedRotation = relativeRotationAngles(relocatedCenter, scaleCenter)
        }

        function centerAtPixel(x, y)
        {
            centerAt(animatedRotationMatrix.times(fisheyeShader.unproject(
                fisheyeShader.pixelToProjection(x, y, animatedScale))))
        }

        // Values modified by user interaction.

        property real scalePower: 0.0
        property vector2d unconstrainedRotation: Qt.vector2d(0.0, 0.0)

        // Animated intermediates.

        readonly property int kAnimationDurationMs: 250

        property real animatedScalePower: scalePower
        Behavior on animatedScalePower
        {
            id: scalePowerAnimationBehavior
            NumberAnimation { duration: interactor.kAnimationDurationMs }
        }

        property real animatedRotationX: unconstrainedRotation.x
        Behavior on animatedRotationX
        {
            enabled: !mouseArea.draggingStarted
                && !pinchArea.zoomStarted && !kineticAnimator.inertia

            RotationAnimation
            {
                direction: RotationAnimation.Shortest
                duration: interactor.kAnimationDurationMs
            }
        }

        property real animatedRotationY: unconstrainedRotation.y
        Behavior on animatedRotationY
        {
            enabled: !mouseArea.draggingStarted
                && !pinchArea.zoomStarted && !kineticAnimator.inertia

            RotationAnimation
            {
                direction: RotationAnimation.Shortest
                duration: interactor.kAnimationDurationMs
            }
        }

        // Internal properties and functions.

        property vector2d previousRotation
        property real previousScalePower
        property vector3d scaleCenter

        function scaleByPower(scalePower)
        {
            return Math.pow(2.0, scalePower)
        }

        function rotationMatrix(degrees)
        {
            switch (viewMode)
            {
                case MediaDewarpingParams.VerticalUp:
                    return Utils3D.rotationZ(Utils3D.radians(degrees.y)).times(
                        Utils3D.rotationX(Utils3D.radians(degrees.x)))

                case MediaDewarpingParams.VerticalDown:
                    return Utils3D.rotationZ(-Utils3D.radians(degrees.y)).times(
                        Utils3D.rotationX(Utils3D.radians(degrees.x)))

                default: // MediaDewarpingParams.Horizontal
                    return Utils3D.rotationY(Utils3D.radians(degrees.y)).times(
                        Utils3D.rotationX(Utils3D.radians(degrees.x)))
            }
        }

        function constrainRotation(unconstrained, scale)
        {
            var limitByEdge = (180.0 - fisheyeShader.fov(scale)) / 2.0
            var limitByCenter = Math.min(fisheyeShader.fov(scale) / 2.0, limitByEdge)

            switch (viewMode)
            {
                case MediaDewarpingParams.VerticalUp:
                    return Qt.vector2d(Math.max(-limitByEdge, Math.min(-limitByCenter, unconstrained.x)),
                        normalizedAngle(unconstrained.y))

                case MediaDewarpingParams.VerticalDown:
                    return Qt.vector2d(Math.max(limitByCenter, Math.min(limitByEdge, unconstrained.x)),
                        normalizedAngle(unconstrained.y))

                default: // MediaDewarpingParams.Horizontal
                    return Qt.vector2d(Math.max(-limitByEdge, Math.min(limitByEdge, unconstrained.x)),
                        Math.max(-limitByEdge, Math.min(limitByEdge, unconstrained.y)))
            }
        }

        function normalizedAngle(degrees) //< Brings angle to [-180, 180] range.
        {
            var angle = degrees % 360
            if (angle < -180)
                return angle + 360
            else if (angle > 180)
                return angle - 360
            else
                return angle
        }

        function centerAt(pointOnSphere)
        {
            unconstrainedRotation = rotationAngles(pointOnSphere)
        }

        function rotationAngles(pointOnSphere)
        {
            switch (viewMode)
            {
                case MediaDewarpingParams.VerticalUp:
                {
                    var alpha = -Utils3D.degrees(Math.acos(-pointOnSphere.z))
                    var beta = -Utils3D.degrees(Math.atan2(pointOnSphere.x, pointOnSphere.y))
                    return Qt.vector2d(alpha, beta)
                }

                case MediaDewarpingParams.VerticalDown:
                {
                    var alpha = Utils3D.degrees(Math.acos(-pointOnSphere.z))
                    var beta = -Utils3D.degrees(Math.atan2(pointOnSphere.x, -pointOnSphere.y))
                    return Qt.vector2d(alpha, beta)
                }

                default: // MediaDewarpingParams.Horizontal
                {
                    var alpha = -Utils3D.degrees(Math.asin(pointOnSphere.y))
                    var beta = -Utils3D.degrees(Math.atan2(pointOnSphere.x, -pointOnSphere.z))
                    return Qt.vector2d(alpha, beta)
                }
            }
        }

		function relativeRotationAngles(from, to)
        {
            switch (viewMode)
            {
                case MediaDewarpingParams.VerticalUp:
                {
                    var scaledY = -Math.sqrt(Math.max(0, to.x * to.x + to.y * to.y - from.x * from.x))
                    var beta = Math.atan2(to.x, -to.y) - Math.atan2(from.x, scaledY)
                    var rotatedTo = Utils3D.rotationZ(-beta).times(to)
                    var alpha = Math.atan2(from.y, -from.z) - Math.atan2(rotatedTo.y, -rotatedTo.z)
                    return Qt.vector2d(Utils3D.degrees(alpha), Utils3D.degrees(beta))
                }

                case MediaDewarpingParams.VerticalDown:
                {
                    var scaledY = -Math.sqrt(Math.max(0, to.x * to.x + to.y * to.y - from.x * from.x))
                    var beta = Math.atan2(to.x, to.y) - Math.atan2(from.x, scaledY)
                    var rotatedTo = Utils3D.rotationZ(beta).times(to)
                    var alpha = Math.atan2(from.y, -from.z) - Math.atan2(rotatedTo.y, -rotatedTo.z)
                    return Qt.vector2d(Utils3D.degrees(alpha), Utils3D.degrees(beta))
                }

                default: // MediaDewarpingParams.Horizontal
                {
                    var scaledZ = Math.sqrt(Math.max(0, to.x * to.x + to.z * to.z - from.x * from.x))
                    var beta = Math.atan2(from.x, scaledZ) - Math.atan2(to.x, -to.z)
                    var rotatedTo = Utils3D.rotationY(-beta).times(to)
                    var alpha = Math.atan2(from.y, -from.z) - Math.atan2(rotatedTo.y, -rotatedTo.z)
                    return Qt.vector2d(Utils3D.degrees(alpha), Utils3D.degrees(beta))
                }
            }
        }
    }

    PinchArea
    {
        id: pinchArea

        anchors.fill: parent
        property bool zoomStarted: false

        onPinchStarted:
        {
            interactor.startZoom(pinch.startCenter.x, pinch.startCenter.y)
            kineticAnimator.interrupt()
            zoomStarted = true
        }

        onPinchUpdated:
            updateZoom(pinch.center.x, pinch.center.y, pinch.scale)

        onPinchFinished:
        {
            updateZoom(pinch.center.x, pinch.center.y, pinch.scale)
            zoomStarted = false
        }

        function updateZoom(x, y, scale)
        {
            interactor.updateZoom(x, y, Math.log(scale) / Math.LN2, false)
        }

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent

            drag.threshold: 10

            property bool draggingStarted
            property real pressX
            property real pressY
            property bool acceptClick
            property point lastClickPosition

            readonly property real pixelRadius: Math.min(width, height) / 2.0

            KineticPositionAnimator
            {
                id: kineticAnimator

                onPositionChanged:
                {
                    const kSensitivity = 100.0
                    var normalization = kSensitivity / mouseArea.pixelRadius
                    var dx = position.x - initialPosition.x
                    var dy = position.y - initialPosition.y
                    interactor.updateRotation(dy * normalization, dx * normalization)
                }
            }

            onPressed:
            {
                pressX = mouse.x
                pressY = mouse.y
                kineticAnimator.interrupt()
            }

            onDoubleClicked:
            {
                var distance = Qt.vector2d(
                    lastClickPosition.x - mouse.x,
                    lastClickPosition.y - mouse.y).length()

                if (distance > drag.threshold)
                    return

                clickFilterTimer.stop()
                scalePowerAnimationBehavior.enabled = true

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
                acceptClick = !draggingStarted
                if (!draggingStarted)
                    return

                clickFilterTimer.stop()
                kineticAnimator.finishMeasurement(mouse.x, mouse.y)
                draggingStarted = false
            }

            onClicked:
            {
                lastClickPosition = Qt.point(mouse.x, mouse.y)
                if (acceptClick)
                    clickFilterTimer.restart()
            }

            Timer
            {
                id: clickFilterTimer

                interval: 200

                onTriggered: content.clicked()
            }

            onPositionChanged:
            {
                if (mouse.buttons & Qt.LeftButton)
                {
                    if (draggingStarted)
                    {
                        kineticAnimator.updateMeasurement(mouse.x, mouse.y)
                    }
                    else
                    {
                        draggingStarted = Math.abs(mouse.x - pressX) > drag.threshold
                            || Math.abs(mouse.y - pressY) > drag.threshold

                        if (draggingStarted)
                        {
                            kineticAnimator.startMeasurement(mouse.x, mouse.y)
                            interactor.startRotation()
                        }
                    }
                }
            }

            onWheel:
            {
                if (draggingStarted)
                    startDrag(wheel.x, wheel.y)

                const kSensitivity = 100.0
                var deltaPower = wheel.angleDelta.y * kSensitivity / 1.0e5

                interactor.startZoom(wheel.x, wheel.y)
                interactor.updateZoom(wheel.x, wheel.y, deltaPower, true)

                if (draggingStarted)
                    interactor.startRotation()
            }
        }
    }
}

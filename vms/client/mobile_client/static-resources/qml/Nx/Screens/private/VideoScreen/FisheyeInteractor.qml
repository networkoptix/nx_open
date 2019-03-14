import QtQuick 2.6
import Nx 1.0
import Nx.Media 1.0

Object
{
    id: interactor

    // Queryables.

    property FisheyeShaderItem fisheyeShader: null

    property bool enableAnimation: true
    readonly property real animatedScale: scaleByPower(animatedScalePower)
    readonly property real currentScale: scaleByPower(scalePower)

    readonly property vector2d animatedRotation: constrainRotation(
        Qt.vector2d(animatedRotationX, animatedRotationY), animatedScale)

    readonly property vector2d currentRotation: constrainRotation(
        Qt.vector2d(unconstrainedRotation.x, unconstrainedRotation.y), currentScale)

    readonly property bool animating:
        scaleAnimation.running
        || xRotationAnimation.running
        || yRotationAnimation.running

    readonly property matrix4x4 animatedRotationMatrix: rotationMatrix(animatedRotation)
    readonly property matrix4x4 currentRotationMatrix: rotationMatrix(currentRotation)
    readonly property var viewMode: fisheyeShader && fisheyeShader.helper
        ? fisheyeShader.helper.fisheyeParams.viewMode
        : MediaDewarpingParams.Horizontal

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

    function updateZoom(x, y, deltaPower)
    {
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

        enabled: interactor.enableAnimation
        NumberAnimation
        {
            id: scaleAnimation
            duration: interactor.kAnimationDurationMs
        }
    }

    property real animatedRotationX: unconstrainedRotation.x
    Behavior on animatedRotationX
    {
        enabled: interactor.enableAnimation

        RotationAnimation
        {
            id: xRotationAnimation
            direction: RotationAnimation.Shortest
            duration: interactor.kAnimationDurationMs
        }
    }

    property real animatedRotationY: unconstrainedRotation.y
    Behavior on animatedRotationY
    {
        enabled: interactor.enableAnimation

        RotationAnimation
        {
            id: yRotationAnimation
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

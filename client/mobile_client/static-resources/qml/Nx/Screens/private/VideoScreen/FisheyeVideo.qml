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

        readonly property real currentScale: Math.pow(2.0, scalePower)

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
                    return Qt.vector2d(Math.max(-limitByEdge, Math.min(-limitByCenter, unboundRotation.x)),
                                       normalizedAngle(unboundRotation.y))

                case MediaDewarpingParams.VerticalDown:
                    return Qt.vector2d(Math.max(limitByCenter, Math.min(limitByEdge, unboundRotation.x)),
                                       normalizedAngle(unboundRotation.y))

                default:
                    return Qt.vector2d(Math.max(-limitByEdge, Math.min(limitByEdge, unboundRotation.x)),
                                       Math.max(-limitByEdge, Math.min(limitByEdge, unboundRotation.y)))
            }
        }

        property vector2d unboundRotation: Qt.vector2d(0.0, 0.0)

        property vector2d previousRotation

        property real scalePower: 0.0 //TODO: #vkutin Animate?

        function startRotation()
        {
            previousRotation = currentRotation
        }

        function updateRotation(aroundX, aroundY) // angle deltas since start, in degrees
        {
            var rotationFactor = fisheyeShader.fov / 180.0

            unboundRotation = previousRotation.plus(Qt.vector2d(
                aroundX * rotationFactor,
                aroundY * rotationFactor))
        }

        function scaleBy(delta)
        {
            var deltaPower = delta / 1.0e5
            scalePower = Math.min(4.0, Math.max(0.0, scalePower + deltaPower))
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

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent

        drag.threshold: 10
 
        property bool draggingStarted
        property int startX
        property int startY

        readonly property real pixelRadius: Math.min(width, height) / 2.0
        readonly property vector2d pixelCenter: Qt.vector2d(width, height).times(0.5)

        onPressed: 
        { 
            startX = mouse.x
            startY = mouse.y
        }

        onReleased:
        {
            if (draggingStarted)
            {
                updateDrag(mouse.x - startX, mouse.y - startY)
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
                if (!draggingStarted)
                {
                    draggingStarted = Math.abs(mouse.x - startX) > drag.threshold 
                                   || Math.abs(mouse.y - startY) > drag.threshold
                    if (draggingStarted)
                    {
                        startX = mouse.x
                        startY = mouse.y
                        interactor.startRotation()
                    }
                }

                if (draggingStarted)
                    updateDrag(mouse.x - startX, mouse.y - startY)
            }
        }

        onWheel:
        {
            const kSensitivity = 100.0
            interactor.scaleBy(wheel.angleDelta.y * kSensitivity)
        }

        function updateDrag(dx, dy)
        {                  
            const kSensitivity = 100.0
            var normalization = kSensitivity / pixelRadius
            interactor.updateRotation(dy * normalization, 
                                      dx * normalization)
        }
    }
}

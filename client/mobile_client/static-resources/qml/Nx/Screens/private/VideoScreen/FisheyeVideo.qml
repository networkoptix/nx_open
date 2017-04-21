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

        viewRotationMatrix: 
        {
            var rotX = Utils3D.rotationX(Utils3D.radians(interactor.currentRotation.x));
            var rotY = Utils3D.rotationY(Utils3D.radians(interactor.currentRotation.y));

            return resourceHelper && resourceHelper.fisheyeParams.viewMode != MediaDewarpingParams.Horizontal
                ? rotX.times(rotY)
                : rotY.times(rotX); 
        }

        viewScale: interactor.currentScale

        viewShift: width > height
            ? Qt.vector2d(0, 0)
            : Qt.vector2d(0, -(1.0 - width / height) / 2 * videoCenterHeightOffsetFactor)
    }

    Object
    {
        id: interactor

        property real currentScale: 1.0;

        property vector2d currentRotation: Qt.vector2d(0.0, 0.0)

        //TODO: #vkutin This can definitely be improved.
        readonly property real rotationFactor: 300 * fisheyeShader.angleScaleFactor 
            / (Math.min(content.width, content.height))

        property vector2d previousRotation
 
        function startRotation()
        {
            previousRotation = currentRotation
        }

        //TODO: #vkutin This also can be improved for floor & ceiling mounts.
        function updateRotation(aroundX, aroundY) // angle deltas since start, in degrees
        {
            currentRotation = Qt.vector2d(
                Math.max(-90, Math.min(90, previousRotation.x + aroundX * rotationFactor)),
                Math.max(-90, Math.min(90, previousRotation.y + aroundY * rotationFactor)));
        }

        function scaleBy(delta)
        {
            currentScale = Math.min(10.0, Math.max(1.0, currentScale * Math.exp(delta / 1000.0)))
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
            interactor.scaleBy(wheel.angleDelta.y)
        }

        function updateDrag(dx, dy)
        {
            interactor.updateRotation(dy * interactor.rotationFactor, 
                                      dx * interactor.rotationFactor)
        }
    }
}

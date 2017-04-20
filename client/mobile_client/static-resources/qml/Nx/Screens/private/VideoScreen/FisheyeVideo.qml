import QtQuick 2.6
import Nx.Core 1.0
import Nx.Media 1.0

Item
{
    id: content

    property alias mediaPlayer: video.player
    property MediaResourceHelper resourceHelper: null

    property real videoCenterHeightOffsetFactor: 1.0 //TODO: #vkutin Use it!

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
        sourceItem: video
        anchors.fill: content

        fieldOffset: resourceHelper
            ? Qt.vector2d(0.5 - resourceHelper.fisheyeParams.xCenter, 0.5 - resourceHelper.fisheyeParams.yCenter)
            : Qt.vector2d(0.0, 0.0)

//TODO: #vkutin implement height factor shift for portrait orientation

        fieldStretch: resourceHelper ? resourceHelper.fisheyeParams.hStretch : 1.0

        fieldRadius: resourceHelper ? resourceHelper.fisheyeParams.radius : 0.0

        fieldRotation: resourceHelper ? resourceHelper.fisheyeParams.fovRot : 0.0

//TODO: #vkutin implement 90-180-270 degrees rotation, either together with camera correction angle or separately
//        customRotation: resourceHelper ? resourceHelper.customRotation : 0
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: content.clicked()
    }
}

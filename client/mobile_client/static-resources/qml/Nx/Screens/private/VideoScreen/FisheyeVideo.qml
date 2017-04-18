import QtQuick 2.6
import QtMultimedia 5.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

VideoPositioner
{
    id: content

    property alias mediaPlayer: video.mediaPlayer
    property alias resourceHelper: video.resourceHelper

    readonly property bool zoomed: false

    signal clicked()

    clip: true

    sourceSize: Qt.size(video.implicitWidth, video.implicitHeight)
    videoRotation: resourceHelper ? resourceHelper.customRotation : 0

    customAspectRatio:
    {
        var aspectRatio = resourceHelper ? resourceHelper.customAspectRatio : 0.0
        if (aspectRatio === 0.0)
        {
            if (mediaPlayer)
                aspectRatio = mediaPlayer.aspectRatio
            else
                aspectRatio = sourceSize.width / sourceSize.height
        }

        var layoutSize = resourceHelper ? resourceHelper.layoutSize : Qt.size(1, 1)
        aspectRatio *= layoutSize.width / layoutSize.height

        return aspectRatio
    }

    item: MultiVideoOutput { id: video }

    function clear()
    {
        video.clear()
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: content.clicked()
    }
}

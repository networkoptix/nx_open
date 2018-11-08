import Nx.Controls 1.0

VideoPositioner
{
    id: content

    property alias resourceHelper: video.resourceHelper
    property alias mediaPlayer: video.mediaPlayer
    property alias videoOutput: video

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

    item: video

    MultiVideoOutput { id: video }
}


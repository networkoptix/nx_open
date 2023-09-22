// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

VideoPositioner
{
    id: content

    property alias resourceHelper: video.resourceHelper
    property alias mediaPlayer: video.mediaPlayer
    property alias videoOutput: video

    sourceSize: (video.implicitWidth > 0 && video.implicitHeight > 0)
        ? Qt.size(video.implicitWidth, video.implicitHeight)
        : Qt.size(width, height)

    videoRotation: resourceHelper ? resourceHelper.customRotation : 0
    customAspectRatio:
    {
        let aspectRatio = resourceHelper ? resourceHelper.customAspectRatio : 0.0
        if (aspectRatio === 0.0)
        {
            if (mediaPlayer && mediaPlayer.loaded)
                aspectRatio = mediaPlayer.aspectRatio
            else if (resourceHelper)
                aspectRatio = resourceHelper.aspectRatio
            else
                aspectRatio = sourceSize.width / sourceSize.height
        }

        const layoutSize = resourceHelper ? resourceHelper.layoutSize : Qt.size(1, 1)
        aspectRatio *= layoutSize.width / layoutSize.height

        return aspectRatio
    }

    item: video

    MultiVideoOutput { id: video }
}

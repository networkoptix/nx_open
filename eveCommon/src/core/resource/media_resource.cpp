#include "media_resource.h"
#include "resource_media_layout.h"
#include "../dataprovider/media_streamdataprovider.h"


//QnDefaultMediaResourceLayout globalDefaultMediaResourceLayout;

QnMediaResource::QnMediaResource(): 
    QnResource()
{
    addFlag(QnResource::media);
}

QnMediaResource::~QnMediaResource()
{
}

QImage QnMediaResource::getImage(int /*channnel*/, QDateTime /*time*/, QnStreamQuality /*quality*/)
{
    return QImage();
}

QnVideoResourceLayout* QnMediaResource::getVideoLayout(QnAbstractMediaStreamDataProvider* reader)
{
    static QnDefaultDeviceVideoLayout defaultLayout;
    if (reader)
        return reader->getVideoLayout();
    else
        return &defaultLayout;
}

QnResourceAudioLayout* QnMediaResource::getAudioLayout(QnAbstractMediaStreamDataProvider* reader)
{
    static QnEmptyAudioLayout defaultLayout;
    if (reader)
        return reader->getAudioLayout();
    else
        return &defaultLayout;
}

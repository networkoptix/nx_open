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

QImage QnMediaResource::getImage(int /*channnel*/, QDateTime /*time*/, QnStreamQuality /*quality*/) const
{
    return QImage();
}

static QnDefaultDeviceVideoLayout videoLayout;
const QnVideoResourceLayout* QnMediaResource::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider) const
{
    return &videoLayout;
}

static QnEmptyAudioLayout audioLayout;
const QnResourceAudioLayout* QnMediaResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider) const
{
    return &audioLayout;
}

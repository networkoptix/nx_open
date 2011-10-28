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

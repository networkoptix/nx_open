#include "media_resource.h"
#include "video_resource_layout.h"


extern QnDefaultDeviceVideoLayout globalDefaultDeviceVideoLayout;

QnMediaResource::QnMediaResource()
{
    addFlag(QnResource::media);
}

QnMediaResource::~QnMediaResource()
{
 
}

QnVideoResoutceLayout* QnMediaResource::getVideoLayout() const
{
    return &globalDefaultDeviceVideoLayout;
}

QnAbstractMediaStreamDataProvider* QnMediaResource::acquireMediaProvider(int number)
{
    return 0;
}

QnAbstractMediaStreamDataProvider* QnMediaResource::getMediaProvider(int number)
{
    return 0;
}

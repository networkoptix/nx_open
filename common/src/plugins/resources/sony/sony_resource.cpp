#include "../onvif/dataprovider/rtp264_stream_provider.h"
#include "sony_resource.h"


const char* QnPlSonyResource::MANUFACTURE = "Sony";


QnPlSonyResource::QnPlSonyResource()
{
    setAuth("admin", "admin");
    
}

bool QnPlSonyResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlSonyResource::updateMACAddress()
{
    return true;
}

QString QnPlSonyResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlSonyResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlSonyResource::createLiveDataProvider()
{
    QString request = "media/video1";

    return new QnRtpStreamReader(toSharedPointer(), request);
}

void QnPlSonyResource::setCropingPhysical(QRect croping)
{

}

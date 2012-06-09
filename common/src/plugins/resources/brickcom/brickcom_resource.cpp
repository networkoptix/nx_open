#include "../onvif/dataprovider/rtp264_stream_provider.h"
#include "brickcom_resource.h"


extern const char* BRICKCOM_MANUFACTURER;
const char* QnPlBrickcomResource::MANUFACTURE = BRICKCOM_MANUFACTURER;


QnPlBrickcomResource::QnPlBrickcomResource()
{
    setAuth("admin", "admin");
    
}

bool QnPlBrickcomResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlBrickcomResource::updateMACAddress()
{
    return true;
}

QString QnPlBrickcomResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlBrickcomResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlBrickcomResource::createLiveDataProvider()
{
    QString request = "ONVIF/channel1";

    return new RTP264StreamReader(toSharedPointer(), request);
}

void QnPlBrickcomResource::setCropingPhysical(QRect croping)
{

}

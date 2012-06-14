#include "../onvif/dataprovider/rtp_stream_provider.h"
#include "brickcom_resource.h"


const char* QnPlBrickcomResource::MANUFACTURE = "Brickcom";


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

    return new QnRtpStreamReader(toSharedPointer(), request);
}

void QnPlBrickcomResource::setCropingPhysical(QRect croping)
{

}

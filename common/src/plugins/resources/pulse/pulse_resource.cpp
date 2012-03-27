
#include "../onvif/dataprovider/rtp264_stream_provider.h"
#include "pulse_resource.h"


const char* QnPlPulseResource::MANUFACTURE = "Pulse";


QnPlPulseResource::QnPlPulseResource()
{
    setAuth("admin", "admin");
}

bool QnPlPulseResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlPulseResource::updateMACAddress()
{
    return true;
}

QString QnPlPulseResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlPulseResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlPulseResource::createLiveDataProvider()
{
    QString request = "0";
    return new RTP264StreamReader(toSharedPointer(), request);
}

void QnPlPulseResource::setCropingPhysical(QRect croping)
{

}

#include "isd_resource.h"
#include "../onvif/dataprovider/rtp264_stream_provider.h"


const char* QnPlIsdResource::MANUFACTURE = "ISD";


QnPlIsdResource::QnPlIsdResource()
{
    setAuth("root", "system");
    
}

bool QnPlIsdResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlIsdResource::updateMACAddress()
{
    return true;
}

QString QnPlIsdResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlIsdResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlIsdResource::createLiveDataProvider()
{
    return new RTP264StreamReader(toSharedPointer(), "stream1");
}

void QnPlIsdResource::setCropingPhysical(QRect croping)
{

}

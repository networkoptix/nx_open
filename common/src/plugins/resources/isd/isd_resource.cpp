#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "../onvif/dataprovider/onvif_h264.h"
#include "isd_resource.h"
#include "isd_stream_reader.h"

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
    return new PlISDStreamReader(toSharedPointer());
}

void QnPlIsdResource::setCropingPhysical(QRect croping)
{

}

#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "../onvif/dataprovider/onvif_h264.h"
#include "ipwebcam_droid_resource.h"

const char* QnPlDriodIpWebCamResource::MANUFACTURE = "NetworkOptixDroid";


QnPlDriodIpWebCamResource::QnPlDriodIpWebCamResource()
{
    setAuth("", "");
    
}

bool QnPlDriodIpWebCamResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlDriodIpWebCamResource::updateMACAddress()
{
    return true;
}

QString QnPlDriodIpWebCamResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlDriodIpWebCamResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlDriodIpWebCamResource::createLiveDataProvider()
{
    return new MJPEGtreamreader(toSharedPointer(), "");
}

void QnPlDriodIpWebCamResource::setCropingPhysical(QRect croping)
{

}

int QnPlDriodIpWebCamResource::httpPort() const
{
    return 8080;
}
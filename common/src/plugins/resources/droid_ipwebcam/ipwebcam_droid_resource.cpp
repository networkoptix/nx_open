#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "ipwebcam_droid_resource.h"
#include "ipwebcam_droid_stream_reader.h"

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

void QnPlDriodIpWebCamResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlDriodIpWebCamResource::createLiveDataProvider()
{
    return new QnPlDroidIpWebCamReader(toSharedPointer());
}

void QnPlDriodIpWebCamResource::setCropingPhysical(QRect /*croping*/)
{

}

int QnPlDriodIpWebCamResource::httpPort() const
{
    return 8080;
}
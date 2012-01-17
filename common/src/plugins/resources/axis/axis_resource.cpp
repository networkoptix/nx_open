#include "axis_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"

const char* QnPlAxisResource::MANUFACTURE = "Axis";


QnPlAxisResource::QnPlAxisResource()
{
    setAuth("root", "1");
    
}

bool QnPlAxisResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlAxisResource::updateMACAddress()
{
    return true;
}

QString QnPlAxisResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlAxisResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlAxisResource::createLiveDataProvider()
{
    return new MJPEGtreamreader(toSharedPointer(), "mjpg/video.mjpg");
}

void QnPlAxisResource::setCropingPhysical(QRect croping)
{

}

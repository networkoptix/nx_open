#ifdef ENABLE_DROID

#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "ipwebcam_droid_resource.h"
#include "ipwebcam_droid_stream_reader.h"

const char* QnPlDriodIpWebCamResource::MANUFACTURE = "NetworkOptixDroid";


QnPlDriodIpWebCamResource::QnPlDriodIpWebCamResource()
{
    setAuth(QString(), QString());
}

bool QnPlDriodIpWebCamResource::isResourceAccessible()
{
    return updateMACAddress();
}

QString QnPlDriodIpWebCamResource::getDriverName() const
{
    return QLatin1String(MANUFACTURE);
}

void QnPlDriodIpWebCamResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlDriodIpWebCamResource::createLiveDataProvider()
{
    return new QnPlDroidIpWebCamReader(toSharedPointer());
}

void QnPlDriodIpWebCamResource::setCroppingPhysical(QRect /*cropping*/)
{

}

int QnPlDriodIpWebCamResource::httpPort() const
{
    return 8080;
}

#endif // #ifdef ENABLE_DROID

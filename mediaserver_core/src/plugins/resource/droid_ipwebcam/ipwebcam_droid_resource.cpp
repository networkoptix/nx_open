#ifdef ENABLE_DROID

#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "ipwebcam_droid_resource.h"
#include "ipwebcam_droid_stream_reader.h"

const QString QnPlDriodIpWebCamResource::MANUFACTURE(lit("NetworkOptixDroid"));


QnPlDriodIpWebCamResource::QnPlDriodIpWebCamResource()
{
}

QString QnPlDriodIpWebCamResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnPlDriodIpWebCamResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

nx::mediaserver::resource::StreamCapabilityMap QnPlDriodIpWebCamResource::getStreamCapabilityMapFromDrives(
    bool primaryStream)
{
    // TODO: implement me
    return nx::mediaserver::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnPlDriodIpWebCamResource::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
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

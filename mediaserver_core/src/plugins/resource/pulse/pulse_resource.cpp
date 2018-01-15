#ifdef ENABLE_PULSE_CAMERA

#include "../onvif/dataprovider/rtp_stream_provider.h"
#include "pulse_resource.h"


const QString QnPlPulseResource::MANUFACTURE(lit("Pulse"));


QnPlPulseResource::QnPlPulseResource()
{
    setVendor(lit("Pulse"));
}

QString QnPlPulseResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnPlPulseResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlPulseResource::createLiveDataProvider()
{
    QString request = QLatin1String("0");
    return new QnRtpStreamReader(toSharedPointer(), request);
}

void QnPlPulseResource::setCroppingPhysical(QRect /*cropping*/)
{

}

nx::mediaserver::resource::StreamCapabilityMap QnPlPulseResource::getStreamCapabilityMapFromDrive(
    bool primaryStream)
{
    // TODO: implement me
    return nx::mediaserver::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnPlPulseResource::initializeCameraDriver()
{
    updateDefaultAuthIfEmpty(QLatin1String("admin"), QLatin1String("admin"));
    return CameraDiagnostics::NoErrorResult();
}


#endif // #ifdef ENABLE_PULSE_CAMERA

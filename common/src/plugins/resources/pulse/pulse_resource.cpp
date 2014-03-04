#ifdef ENABLE_PULSE_CAMERA

#include "../onvif/dataprovider/rtp_stream_provider.h"
#include "pulse_resource.h"


const char* QnPlPulseResource::MANUFACTURE = "Pulse";


QnPlPulseResource::QnPlPulseResource()
{
    setVendor(lit("Pulse"));
    setAuth(QLatin1String("admin"), QLatin1String("admin"));
}

bool QnPlPulseResource::isResourceAccessible()
{
    return updateMACAddress();
}

QString QnPlPulseResource::getDriverName() const
{
    return QLatin1String(MANUFACTURE);
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

#endif // #ifdef ENABLE_PULSE_CAMERA

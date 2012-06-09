#include "digital_watchdog_resource.h"
#include "digital_watchdog_stream_reader.h"


extern const char* DIGITALWATCHDOG_MANUFACTURER;
const char* QnPlWatchDogResource::MANUFACTURE = DIGITALWATCHDOG_MANUFACTURER;


QnPlWatchDogResource::QnPlWatchDogResource()
{
    setAuth("admin", "admin");
}

bool QnPlWatchDogResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlWatchDogResource::updateMACAddress()
{
    return true;
}

QString QnPlWatchDogResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlWatchDogResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

QnAbstractStreamDataProvider* QnPlWatchDogResource::createLiveDataProvider()
{
    return new QnPlDWDStreamReader(toSharedPointer());
}

void QnPlWatchDogResource::setCropingPhysical(QRect /*croping*/)
{
}

void QnPlWatchDogResource::init() 
{
    //QnPlWatchDogResource::

    //

    //cameraname1=1CH&cameraname2=2CH&resolution1=1920:1080&resolution2=960:544&codec1=h264&codec2=mjpeg&bitrate1=4000&bitrate2=4000&framerate1=30&framerate2=30&quality1=2&quality2=2
}

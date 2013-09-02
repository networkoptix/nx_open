#include "desktop_camera_resource.h"
#include "desktop_camera_reader.h"

const char* QnDesktopCameraResource::MANUFACTURE = "VIRTUAL_CAMERA";

QString QnDesktopCameraResource::getDriverName() const
{
    return QLatin1String(MANUFACTURE);
}

QnDesktopCameraResource::QnDesktopCameraResource(): QnPhysicalCameraResource()
{

}

QnDesktopCameraResource::~QnDesktopCameraResource()
{

}

bool QnDesktopCameraResource::setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS)
{
    return false;
}

bool QnDesktopCameraResource::isResourceAccessible()
{
    return true;
}

QnAbstractStreamDataProvider* QnDesktopCameraResource::createLiveDataProvider()
{
    return new QnDesktopCameraStreamReader(toSharedPointer());
}

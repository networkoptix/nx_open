#include "desktop_camera_resource.h"
#include "desktop_camera_reader.h"

const char* QnDesktopCameraResource::MANUFACTURE = "VIRTUAL_CAMERA";

static QByteArray ID_PREFIX("Desktop_camera_");

QString QnDesktopCameraResource::getDriverName() const
{
    return QLatin1String(MANUFACTURE);
}

QnDesktopCameraResource::QnDesktopCameraResource(): QnPhysicalCameraResource()
{
    setFlags(flags() | no_last_gop);
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

QString QnDesktopCameraResource::gePhysicalIdPrefix() const
{
    return lit(ID_PREFIX);
}

QString QnDesktopCameraResource::getUserName() const 
{ 
    return getPhysicalId().mid(ID_PREFIX.size());
}

#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_resource.h"
#include "desktop_camera_reader.h"
#include <media_server/serverutil.h>

const QString QnDesktopCameraResource::MANUFACTURE("VIRTUAL_CAMERA");

static QByteArray ID_PREFIX("Desktop_camera_");

QString QnDesktopCameraResource::getDriverName() const
{
    return MANUFACTURE;
}

QnDesktopCameraResource::QnDesktopCameraResource(): QnPhysicalCameraResource()
{
    setFlags(flags() | no_last_gop);
    setDisabled(true);  // #3087
}

QnDesktopCameraResource::~QnDesktopCameraResource()
{

}

bool QnDesktopCameraResource::setRelayOutputState(const QString& outputID, bool activate, unsigned int autoResetTimeoutMS)
{
    Q_UNUSED(outputID)
    Q_UNUSED(activate)
    Q_UNUSED(autoResetTimeoutMS)
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
    return QLatin1String(ID_PREFIX) + serverGuid() + lit("_");
}

QString QnDesktopCameraResource::getUserName() const 
{ 
    return getPhysicalId().mid(gePhysicalIdPrefix().size());
}

QnConstResourceAudioLayoutPtr QnDesktopCameraResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    const QnDesktopCameraStreamReader* deskopReader = dynamic_cast<const QnDesktopCameraStreamReader*>(dataProvider);
    if (deskopReader && deskopReader->getDPAudioLayout())
        return deskopReader->getDPAudioLayout();
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

#endif //ENABLE_DESKTOP_CAMERA

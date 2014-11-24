#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_resource.h"
#include "desktop_camera_reader.h"
#include <media_server/serverutil.h>

const QString QnDesktopCameraResource::MANUFACTURE("VIRTUAL_CAMERA");

QString QnDesktopCameraResource::getDriverName() const
{
    return MANUFACTURE;
}

QnDesktopCameraResource::QnDesktopCameraResource(): QnPhysicalCameraResource() {
    setFlags(flags() | Qn::no_last_gop | Qn::desktop_camera);
}


QnDesktopCameraResource::QnDesktopCameraResource(const QString &userName): QnPhysicalCameraResource()
{
    setFlags(flags() | Qn::no_last_gop | Qn::desktop_camera);
    setName(userName);
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

QnAbstractStreamDataProvider* QnDesktopCameraResource::createLiveDataProvider()
{
    return new QnDesktopCameraStreamReader(toSharedPointer());
}

QnConstResourceAudioLayoutPtr QnDesktopCameraResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    const QnDesktopCameraStreamReader* deskopReader = dynamic_cast<const QnDesktopCameraStreamReader*>(dataProvider);
    if (deskopReader && deskopReader->getDPAudioLayout())
        return deskopReader->getDPAudioLayout();
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

bool QnDesktopCameraResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) {
    bool result = base_type::mergeResourcesIfNeeded(source);

    if (getName() != source->getName()) {
        setName(source->getName());
        result = true;
    }

    return result;
}

#endif //ENABLE_DESKTOP_CAMERA

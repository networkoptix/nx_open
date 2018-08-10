#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_resource.h"

#include <plugins/resource/desktop_camera/desktop_camera_reader.h>
#include <plugins/resource/desktop_camera/desktop_camera_resource_searcher.h>

#include <media_server/serverutil.h>

const QString QnDesktopCameraResource::MANUFACTURE("VIRTUAL_CAMERA");

QString QnDesktopCameraResource::getDriverName() const
{
    return MANUFACTURE;
}

QnDesktopCameraResource::QnDesktopCameraResource(QnMediaServerModule* serverModule):
    nx::mediaserver::resource::Camera(serverModule)
{
    setFlags(flags() | Qn::no_last_gop | Qn::desktop_camera);
}

QnDesktopCameraResource::QnDesktopCameraResource(QnMediaServerModule* serverModule, const QString& userName):
    nx::mediaserver::resource::Camera(serverModule)
{
    setFlags(flags() | Qn::no_last_gop | Qn::desktop_camera);
    setName(userName);
}

QnDesktopCameraResource::~QnDesktopCameraResource()
{
}

bool QnDesktopCameraResource::setRelayOutputState(const QString& /*outputID*/, bool /*activate*/,
    unsigned int /*autoResetTimeoutMS*/)
{
    return false;
}

QnAbstractStreamDataProvider* QnDesktopCameraResource::createLiveDataProvider()
{
    return new QnDesktopCameraStreamReader(serverModule(), toSharedPointer(this));
}

bool QnDesktopCameraResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr& source)
{
    bool result = base_type::mergeResourcesIfNeeded(source);

    if (getName() != source->getName())
    {
        setName(source->getName());
        result = true;
    }

    return result;
}

bool QnDesktopCameraResource::isReadyToDetach() const
{
    if (!QnDesktopCameraResourceSearcher::instance())
        return true;

    auto camera = this->toSharedPointer().dynamicCast<QnDesktopCameraResource>();
    return !QnDesktopCameraResourceSearcher::instance()->isCameraConnected(camera);  // check if we have already lost connection
}

nx::mediaserver::resource::StreamCapabilityMap
    QnDesktopCameraResource::getStreamCapabilityMapFromDrives(Qn::StreamIndex)
{
    return nx::mediaserver::resource::StreamCapabilityMap(); //< Not used.
}

CameraDiagnostics::Result QnDesktopCameraResource::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
}

#endif //ENABLE_DESKTOP_CAMERA

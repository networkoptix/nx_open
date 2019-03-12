#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_resource.h"

#include <plugins/resource/desktop_camera/desktop_camera_reader.h>
#include <plugins/resource/desktop_camera/desktop_camera_resource_searcher.h>

#include <media_server/serverutil.h>
#include <media_server/media_server_module.h>
#include <media_server/media_server_resource_searchers.h>

const QString QnDesktopCameraResource::MANUFACTURE("VIRTUAL_CAMERA");

QString QnDesktopCameraResource::getDriverName() const
{
    return MANUFACTURE;
}

QnDesktopCameraResource::QnDesktopCameraResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule)
{
    setFlags(flags() | Qn::no_last_gop | Qn::desktop_camera);
}

QnDesktopCameraResource::QnDesktopCameraResource(QnMediaServerModule* serverModule, const QString& userName):
    nx::vms::server::resource::Camera(serverModule)
{
    setFlags(flags() | Qn::no_last_gop | Qn::desktop_camera);
    setName(userName);
}

QnDesktopCameraResource::~QnDesktopCameraResource()
{
}

bool QnDesktopCameraResource::setOutputPortState(const QString& /*outputID*/, bool /*activate*/,
    unsigned int /*autoResetTimeoutMS*/)
{
    return false;
}

QnAbstractStreamDataProvider* QnDesktopCameraResource::createLiveDataProvider()
{
    return new QnDesktopCameraStreamReader(toSharedPointer(this));
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
    auto desktopCameraSearcher =
        serverModule()->resourceSearchers()->searcher<QnDesktopCameraResourceSearcher>();

    if (!desktopCameraSearcher)
        return true;

    auto camera = this->toSharedPointer().dynamicCast<QnDesktopCameraResource>();
    return !desktopCameraSearcher->isCameraConnected(camera);  // check if we have already lost connection
}

CameraDiagnostics::Result QnDesktopCameraResource::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
}

#endif //ENABLE_DESKTOP_CAMERA

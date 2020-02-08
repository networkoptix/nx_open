#include "data_only_camera.h"

namespace nx::vms::server::resource {

DataOnlyCamera::DataOnlyCamera(QnMediaServerModule* serverModule, const QnUuid& resourceTypeId):
    Camera(serverModule)
{
    setTypeId(resourceTypeId);
}

QnAbstractStreamDataProvider* DataOnlyCamera::createLiveDataProvider()
{
    return nullptr;
}

QString DataOnlyCamera::getDriverName() const
{
    return "DRIVER_NOT_FOUND";
}

CameraDiagnostics::Result DataOnlyCamera::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
}

} // namespace nx::vms::server::resource

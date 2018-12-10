#if defined (ENABLE_ONVIF)

#include "lilin_resource.h"

#include <plugins/resource/lilin/lilin_remote_archive_manager.h>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace vms::server {
namespace plugins {

LilinResource::LilinResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule)
{
}

CameraDiagnostics::Result LilinResource::initializeCameraDriver()
{
    auto result = base_type::initializeCameraDriver();

    if (result.errorCode != CameraDiagnostics::ErrorCode::Value::noError)
        return result;

    setCameraCapability(Qn::RemoteArchiveCapability, true);
    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

nx::core::resource::AbstractRemoteArchiveManager* LilinResource::remoteArchiveManager()
{
    if (!m_remoteArchiveManager)
        m_remoteArchiveManager = std::make_unique<LilinRemoteArchiveManager>(this);

    return m_remoteArchiveManager.get();
}

} // namespace plugins
} // namespace vms::server
} // namespace nx

#endif // ENABLE_ONVIF

#include "lilin_resource.h"

#include <plugins/resource/lilin/lilin_remote_archive_manager.h>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

LilinResource::LilinResource():
    QnPlOnvifResource()
{

}

LilinResource::~LilinResource()
{

}

CameraDiagnostics::Result LilinResource::initInternal()
{
    auto result = base_type::initInternal();

    if (result.errorCode != CameraDiagnostics::ErrorCode::Value::noError)
        return result;

    setCameraCapability(Qn::RemoteArchiveCapability, true);

    return CameraDiagnostics::NoErrorResult();
}


nx::core::resource::AbstractRemoteArchiveManager* LilinResource::remoteArchiveManager()
{
    if (!m_remoteArchiveManager)
        m_remoteArchiveManager = std::make_unique<LilinRemoteArchiveManager>(this);

    return m_remoteArchiveManager.get();
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
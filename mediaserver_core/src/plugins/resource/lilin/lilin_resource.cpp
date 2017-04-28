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
    base_type::initInternal();

    setCameraCapability(Qn::RemoteArchiveCapability, true);
    //QnConcurrent::run([&](){synchronizeArchive();});

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
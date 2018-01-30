#pragma once

#if defined(ENABLE_ONVIF)

#include <plugins/resource/onvif/onvif_resource.h>
#include <core/resource/abstract_remote_archive_manager.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class LilinResource: public QnPlOnvifResource
{
    using base_type = QnPlOnvifResource;
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;

public:
    LilinResource();
    virtual ~LilinResource() override;

    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual nx::core::resource::AbstractRemoteArchiveManager* remoteArchiveManager() override;

private:
    std::unique_ptr<AbstractRemoteArchiveManager> m_remoteArchiveManager;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // ENABLE_ONVIF

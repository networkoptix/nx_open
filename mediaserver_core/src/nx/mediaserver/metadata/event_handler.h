#pragma once

#include <core/resource/resource.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class EventHandler: public nx::sdk::metadata::AbstractMetadataHandler
{
public:
    virtual void handleMetadata(
        nx::sdk::Error error,
        nx::sdk::metadata::AbstractMetadataPacket* metadata) override;

    void setResource(const QnResourcePtr& resource);

    void setPluginId(const QnUuid& pluginId);

private:
    QnResourcePtr m_resource;
    QnUuid m_pluginId;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
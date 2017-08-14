#pragma once

#include <plugins/metadata/abstract_metadata_manager.h>
#include <core/resource/resource.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class EventHandler: public nx::sdk::metadata::AbstractMetadataHandler
{
public:
    virtual void handleMetadata(
        nx::sdk::metadata::Error error,
        nx::sdk::metadata::AbstractMetadataPacket* metadata) override;

    void setResource(const QnResourcePtr& resource);

    void setPluginId(const QnUuid& pluginId);

private:
    // TODO: #dmishin move this method to utils
    QnUuid fromPluginGuidToQnUuid(nxpl::NX_GUID& guid);

private:
    QnResourcePtr m_resource;
    QnUuid m_pluginId;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
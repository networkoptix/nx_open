#pragma once

#include <QtCore/QMap>

#include <core/resource/resource.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/vms/event/event_fwd.h>
#include <plugins/plugin_tools.h>

#include <nx/sdk/metadata/abstract_event_metadata_packet.h>
#include <nx/sdk/metadata/abstract_detection_metadata_packet.h>

class QnAbstractDataReceptor;

namespace nx {
namespace mediaserver {
namespace metadata {

class MetadataHandler: public nx::sdk::metadata::MetadataHandler
{
public:
    // TODO: #mike: Separate error handling from metadata handling.
    virtual void handleMetadata(
        nx::sdk::Error error,
        nx::sdk::metadata::MetadataPacket* metadata) override;

    void setResource(const QnSecurityCamResourcePtr& resource);

    void setPluginId(const QnUuid& pluginId);

    void registerDataReceptor(QnAbstractDataReceptor* dataReceptor);
    void removeDataReceptor(QnAbstractDataReceptor* dataReceptor);

    virtual int getParamValue(
       const char* paramName, char* valueBuf, int* valueBufSize) const override;

private:
    nx::vms::event::EventState lastEventState(const QnUuid& eventId) const;

    void setLastEventState(const QnUuid& eventId, nx::vms::event::EventState eventState);

    void handleEventsPacket(
        nxpt::ScopedRef<nx::sdk::metadata::AbstractEventMetadataPacket> packet);

    void handleObjectsPacket(
        nxpt::ScopedRef<nx::sdk::metadata::AbstractObjectsMetadataPacket> packet);

    void handleMetadataEvent(
        nxpt::ScopedRef<nx::sdk::metadata::AbstractDetectedEvent> eventData,
        qint64 timestampUsec);

private:
    QnSecurityCamResourcePtr m_resource;
    QnUuid m_pluginId;
    QMap<QnUuid, nx::vms::event::EventState> m_eventStateMap;
    QnAbstractDataReceptor* m_dataReceptor = nullptr;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx

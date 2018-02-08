#pragma once

#include <QtCore/QMap>

#include <core/resource/resource.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/api/analytics/driver_manifest.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class EventHandler: public nx::sdk::metadata::MetadataHandler
{
public:
    virtual void handleMetadata(
        nx::sdk::Error error,
        nx::sdk::metadata::MetadataPacket* metadata) override;

    void setResource(const QnSecurityCamResourcePtr& resource);

    void setManifest(const nx::api::AnalyticsDriverManifest& manifest);

private:
    nx::vms::event::EventState lastEventState(const QnUuid& eventId) const;
    void setLastEventState(const QnUuid& eventId, nx::vms::event::EventState eventState);
    nx::api::Analytics::EventType eventDescriptor(const QnUuid& eventId) const;
private:
    QnSecurityCamResourcePtr m_resource;
    nx::api::AnalyticsDriverManifest m_manifest;
    QMap<QnUuid, nx::vms::event::EventState> m_eventStateMap;

};

} // namespace metadata
} // namespace mediaserver
} // namespace nx

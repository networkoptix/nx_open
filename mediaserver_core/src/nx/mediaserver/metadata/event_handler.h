#pragma once

#include <QtCore/QMap>

#include <core/resource/resource.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/events/events_fwd.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class EventHandler:
    public QObject,
    public nx::sdk::metadata::AbstractMetadataHandler
{
    Q_OBJECT

public:
    EventHandler();

    virtual void handleMetadata(
        nx::sdk::Error error,
        nx::sdk::metadata::AbstractMetadataPacket* metadata) override;

    void setResource(const QnSecurityCamResourcePtr& resource);

    void setPluginId(const QnUuid& pluginId);

signals:
    void sdkEventTriggered(const nx::vms::event::AnalyticsSdkEventPtr& event);

private:
    nx::vms::event::EventState lastEventState(const QnUuid& eventId) const;
    void setLastEventState(const QnUuid& eventId, nx::vms::event::EventState eventState);

private:
    QnSecurityCamResourcePtr m_resource;
    QnUuid m_pluginId;
    QMap<QnUuid, nx::vms::event::EventState> m_eventStateMap;

};

} // namespace metadata
} // namespace mediaserver
} // namespace nx

#pragma once

#include <optional>

#include <QtCore/QMap>

#include <core/resource/resource_fwd.h>

#include <nx/analytics/types.h>

#include <nx/sdk/helpers/ptr.h>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/events/events_fwd.h>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>

#include <nx/debugging/abstract_visual_metadata_debugger.h>
#include <nx/vms/server/server_module_aware.h>

class QnAbstractDataReceptor;

namespace nx::vms::server::analytics {

class MetadataHandler:
    public QObject,
    public ServerModuleAware
{
    Q_OBJECT

public:
    using DescriptorMap = std::map<QString, nx::vms::api::analytics::EventTypeDescriptor>;

    MetadataHandler(QnMediaServerModule* serverModule);

    void handleMetadata(nx::sdk::analytics::IMetadataPacket* metadata);

    void setResource(QnVirtualCameraResourcePtr resource);
    void setEngineId(QnUuid pluginId);
    void setEventTypeDescriptors(DescriptorMap descriptors);
    void setMetadataSink(QnAbstractDataReceptor* metadataSink);
    void removeMetadataSink(QnAbstractDataReceptor* metadataSink);

    void setVisualDebugger(nx::debugging::AbstractVisualMetadataDebugger* visualDebugger);

signals:
    void sdkEventTriggered(const nx::vms::event::AnalyticsSdkEventPtr& event);

private:
    nx::vms::api::EventState lastEventState(const QString& eventTypeId) const;

    void setLastEventState(const QString& eventTypeId, nx::vms::api::EventState eventState);

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> eventTypeDescriptor(
        const QString& eventTypeId) const;

    void handleEventsPacket(
        nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket> packet);

    void handleObjectsPacket(
        nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket> packet);

    void handleMetadataEvent(
        nx::sdk::Ptr<nx::sdk::analytics::IEvent> eventData,
        qint64 timestampUsec);

private:
    QnVirtualCameraResourcePtr m_resource;
    QnUuid m_engineId;
    nx::analytics::EventTypeDescriptorMap m_eventTypeDescriptors;
    QMap<QString, nx::vms::api::EventState> m_eventStateMap;
    QnAbstractDataReceptor* m_metadataSink = nullptr;
    nx::debugging::AbstractVisualMetadataDebugger* m_visualDebugger = nullptr;
};

} // namespace nx::vms::server::analytics

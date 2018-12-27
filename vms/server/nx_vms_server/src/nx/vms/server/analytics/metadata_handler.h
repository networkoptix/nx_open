#pragma once

#include <QtCore/QMap>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/vms/event/event_fwd.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/common/ptr.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/event/events/events_fwd.h>

#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>

#include <nx/debugging/abstract_visual_metadata_debugger.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/analytics/types.h>

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
    void setPluginId(QString pluginId);
    void setEventTypeDescriptors(DescriptorMap descriptors);
    void setMetadataSink(QnAbstractDataReceptor* metadataSink);
    void removeMetadataSink(QnAbstractDataReceptor* metadataSink);

    void setVisualDebugger(nx::debugging::AbstractVisualMetadataDebugger* visualDebugger);

signals:
    void sdkEventTriggered(const nx::vms::event::AnalyticsSdkEventPtr& event);

private:
    nx::vms::api::EventState lastEventState(const QString& eventTypeId) const;

    void setLastEventState(const QString& eventTypeId, nx::vms::api::EventState eventState);

    nx::vms::api::analytics::EventTypeDescriptor eventTypeDescriptor(
        const QString& eventTypeId) const;

    void handleEventsPacket(
        nx::sdk::common::Ptr<nx::sdk::analytics::IEventMetadataPacket> packet);

    void handleObjectsPacket(
        nx::sdk::common::Ptr<nx::sdk::analytics::IObjectMetadataPacket> packet);

    void handleMetadataEvent(
        nx::sdk::common::Ptr<nx::sdk::analytics::IEvent> eventData,
        qint64 timestampUsec);

private:
    QnVirtualCameraResourcePtr m_resource;
    QString m_pluginId;
    nx::analytics::EventTypeDescriptorMap m_eventTypeDescriptors;
    QMap<QString, nx::vms::api::EventState> m_eventStateMap;
    QnAbstractDataReceptor* m_metadataSink = nullptr;
    nx::debugging::AbstractVisualMetadataDebugger* m_visualDebugger = nullptr;
};

} // namespace nx::vms::server::analytics

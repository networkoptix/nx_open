#pragma once

#include <QtCore/QMap>

#include <core/resource/resource.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/vms/event/event_fwd.h>
#include <plugins/plugin_tools.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/event/events/events_fwd.h>

#include <nx/sdk/analytics/objects_metadata_packet.h>
#include <nx/sdk/analytics/events_metadata_packet.h>

#include <nx/debugging/abstract_visual_metadata_debugger.h>
#include <nx/mediaserver/server_module_aware.h>

class QnAbstractDataReceptor;

namespace nx::mediaserver::analytics {

class MetadataHandler:
    public QObject,
    public nx::sdk::analytics::MetadataHandler,
    public ServerModuleAware
{
    Q_OBJECT

public:
    MetadataHandler(QnMediaServerModule* serverModule);

    virtual void handleMetadata(
        nx::sdk::Error error,
        nx::sdk::analytics::MetadataPacket* metadata) override;

    void setResource(const QnSecurityCamResourcePtr& resource);

    void setManifest(const nx::vms::api::analytics::EngineManifest& manifest);

    void setMetadataSink(QnAbstractDataReceptor* metadataSink);
    void removeMetadataSink(QnAbstractDataReceptor* metadataSink);

    void setVisualDebugger(nx::debugging::AbstractVisualMetadataDebugger* visualDebugger);

signals:
    void sdkEventTriggered(const nx::vms::event::AnalyticsSdkEventPtr& event);

private:
    nx::vms::api::EventState lastEventState(const QString& eventTypeId) const;

    void setLastEventState(const QString& eventTypeId, nx::vms::api::EventState eventState);

    nx::vms::api::analytics::EventType eventTypeDescriptor(const QString& eventTypeId) const;

    void handleEventsPacket(
        nxpt::ScopedRef<nx::sdk::analytics::EventsMetadataPacket> packet);

    void handleObjectsPacket(
        nxpt::ScopedRef<nx::sdk::analytics::ObjectsMetadataPacket> packet);

    void handleMetadataEvent(
        nxpt::ScopedRef<nx::sdk::analytics::Event> eventData,
        qint64 timestampUsec);

private:
    QnSecurityCamResourcePtr m_resource;
    nx::vms::api::analytics::EngineManifest m_manifest;
    QMap<QString, nx::vms::api::EventState> m_eventStateMap;
    QnAbstractDataReceptor* m_metadataSink = nullptr;
    nx::debugging::AbstractVisualMetadataDebugger* m_visualDebugger = nullptr;
};

} // namespace nx::mediaserver::analytics

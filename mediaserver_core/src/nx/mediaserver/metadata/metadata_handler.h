#pragma once

#include <QtCore/QMap>

#include <core/resource/resource.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/vms/event/event_fwd.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/vms/event/events/events_fwd.h>

#include <nx/sdk/metadata/objects_metadata_packet.h>
#include <nx/sdk/metadata/events_metadata_packet.h>

#include <nx/debugging/abstract_visual_metadata_debugger.h>

class QnAbstractDataReceptor;

namespace nx {
namespace mediaserver {
namespace metadata {

class MetadataHandler:
    public QObject,
    public nx::sdk::metadata::MetadataHandler
{
    Q_OBJECT

public:
    MetadataHandler();
    virtual void handleMetadata(
        nx::sdk::Error error,
        nx::sdk::metadata::MetadataPacket* metadata) override;

    void setResource(const QnSecurityCamResourcePtr& resource);

    void setManifest(const nx::api::AnalyticsDriverManifest& manifest);

    void registerDataReceptor(QnAbstractDataReceptor* dataReceptor);
    void removeDataReceptor(QnAbstractDataReceptor* dataReceptor);

    void setVisualDebugger(nx::debugging::AbstractVisualMetadataDebugger* visualDebugger);

signals:
    void sdkEventTriggered(const nx::vms::event::AnalyticsSdkEventPtr& event);

private:
    nx::vms::api::EventState lastEventState(const QnUuid& eventId) const;

    void setLastEventState(const QnUuid& eventId, nx::vms::api::EventState eventState);
    nx::api::Analytics::EventType eventDescriptor(const QnUuid& eventId) const;
    void handleEventsPacket(
        nxpt::ScopedRef<nx::sdk::metadata::EventsMetadataPacket> packet);

    void handleObjectsPacket(
        nxpt::ScopedRef<nx::sdk::metadata::ObjectsMetadataPacket> packet);

    void handleMetadataEvent(
        nxpt::ScopedRef<nx::sdk::metadata::Event> eventData,
        qint64 timestampUsec);

private:
    QnSecurityCamResourcePtr m_resource;
    nx::api::AnalyticsDriverManifest m_manifest;
    QMap<QnUuid, nx::vms::api::EventState> m_eventStateMap;
    QnAbstractDataReceptor* m_dataReceptor = nullptr;
    nx::debugging::AbstractVisualMetadataDebugger* m_visualDebugger = nullptr;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx

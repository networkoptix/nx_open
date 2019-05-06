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
#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>

#include <nx/debugging/abstract_visual_metadata_debugger.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/analytics/metadata_logger.h>

class QnAbstractDataReceptor;

namespace nx::vms::server::analytics {

class MetadataHandler:
    public QObject,
    public ServerModuleAware
{
    Q_OBJECT

public:
    using DescriptorMap = std::map<QString, nx::vms::api::analytics::EventTypeDescriptor>;

    MetadataHandler(
        QnMediaServerModule* serverModule,
        QnVirtualCameraResourcePtr device,
        QnUuid engineId);

    void handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket);
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

    void handleEventMetadataPacket(
        const nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>& eventMetadataPacket);

    void handleObjectMetadataPacket(
        const nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket>& objectMetadataPacket);

    void handleObjectTrackBestShotPacket(
        const nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>&
            objectTrackBestShotPacket);

    void handleEventMetadata(
        const nx::sdk::Ptr<const nx::sdk::analytics::IEventMetadata>& eventMetadata,
        qint64 timestampUsec);

private:
    QnVirtualCameraResourcePtr m_resource;
    QnUuid m_engineId;
    nx::analytics::EventTypeDescriptorMap m_eventTypeDescriptors;
    QMap<QString, nx::vms::api::EventState> m_eventStateMap;
    QnAbstractDataReceptor* m_metadataSink = nullptr;
    nx::debugging::AbstractVisualMetadataDebugger* m_visualDebugger = nullptr;
    nx::analytics::MetadataLogger m_metadataLogger;
};

} // namespace nx::vms::server::analytics

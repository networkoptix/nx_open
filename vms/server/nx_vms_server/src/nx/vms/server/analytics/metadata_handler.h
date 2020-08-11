#pragma once

#include <functional>
#include <optional>

#include <QtCore/QMap>

#include <core/resource/resource_fwd.h>

#include <nx/analytics/types.h>

#include <nx/sdk/ptr.h>

#include <nx/vms/event/events/events_fwd.h>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>
#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/resource/camera.h>

#include <nx/vms/server/analytics/types.h>
#include <nx/vms/server/analytics/object_coordinates_translator.h>
#include <nx/vms/server/analytics/object_track_best_shot_resolver.h>
#include <nx/vms/server/analytics/object_track_best_shot_proxy.h>
#include <nx/vms/server/analytics/wrappers/types.h>

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
    using ViolationHandler = std::function<void(const wrappers::Violation&)>;

    MetadataHandler(
        QnMediaServerModule* serverModule,
        resource::CameraPtr device,
        QnUuid engineId,
        ViolationHandler violationHandler);

    virtual ~MetadataHandler();

    void handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket);
    void setMetadataSinks(MetadataSinkSet metadataSinks);

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
        const nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket0>&
            objectTrackBestShotPacket);

    void handleObjectTrackBestShotPacketWithImage(
        QnUuid trackId,
        const nx::common::metadata::ObjectMetadataPacketPtr& bestShotPacket,
        const nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>&
            sdkBestShotPacketWithImage);

    void handleEventMetadata(
        const nx::sdk::Ptr<const nx::sdk::analytics::IEventMetadata0>& eventMetadata,
        qint64 timestampUsec);

    void postprocessObjectMetadataPacket(
        const nx::common::metadata::ObjectMetadataPacketPtr& packet,
        nx::common::metadata::ObjectMetadataType objectMetadataType);

    void pushObjectMetadataToSinks(
        const nx::common::metadata::ObjectMetadataPacketPtr& packet);

private:
    nx::Mutex m_mutex;
    resource::CameraPtr m_resource;
    QnUuid m_engineId;
    const ViolationHandler m_violationHandler;
    mutable std::optional<nx::analytics::EventTypeDescriptorMap> m_eventTypeDescriptors;
    QMap<QString, nx::vms::api::EventState> m_eventStateMap;
    MetadataSinkSet m_metadataSinks;
    nx::analytics::MetadataLogger m_metadataLogger;
    ObjectCoordinatesTranslator m_objectCoordinatesTranslator;
    bool m_translateObjectBoundingBoxes = true;
    bool m_needToAutoGenerateBestShots = true;

    ObjectTrackBestShotResolver m_objectTrackBestShotResolver;
    ObjectTrackBestShotProxy m_objectTrackBestShotProxy;
};

} // namespace nx::vms::server::analytics

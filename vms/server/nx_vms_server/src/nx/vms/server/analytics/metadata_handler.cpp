#include "metadata_handler.h"

#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>

#include <nx/sdk/ptr.h>
#include <nx/vms_server_plugins/utils/uuid.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <common/common_module.h>
#include <core/resource/security_cam_resource.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <analytics/common/object_metadata.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <media_server/media_server_module.h>

#include <core/resource/camera_resource.h>

#include <nx/analytics/analytics_logging_ini.h>
#include <nx/analytics/event_type_descriptor_manager.h>

namespace nx {
namespace vms::server {
namespace analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms::api::analytics;
using namespace nx::vms_server_plugins::utils;

MetadataHandler::MetadataHandler(
    QnMediaServerModule* serverModule,
    resource::CameraPtr device,
    QnUuid engineId)
    :
    ServerModuleAware(serverModule),
    m_resource(device),
    m_engineId(engineId),
    m_metadataLogger("outgoing_metadata_", m_resource->getId(), m_engineId),
    m_objectCoordinatesTranslator(device)
{
    const auto engineResource =
        resourcePool()->getResourceById<resource::AnalyticsEngineResource>(m_engineId);

    if (!NX_ASSERT(engineResource))
        return;

    const EngineManifest engineManifest = engineResource->manifest();
    m_translateObjectBoundingBoxes = !engineManifest.capabilities.testFlag(
        EngineManifest::Capability::keepObjectBoundingBoxRotation);

    connect(this,
        &MetadataHandler::sdkEventTriggered,
        serverModule->eventConnector(),
        &event::EventConnector::at_analyticsSdkEvent,
        Qt::QueuedConnection);
}

std::optional<EventTypeDescriptor> MetadataHandler::eventTypeDescriptor(
    const QString& eventTypeId) const
{
    if (!m_eventTypeDescriptors)
    {
        m_eventTypeDescriptors = serverModule()->commonModule()
            ->analyticsEventTypeDescriptorManager()
            ->supportedEventTypeDescriptors(m_resource);
    }

    if (const auto it = m_eventTypeDescriptors->find(eventTypeId);
        it != m_eventTypeDescriptors->cend())
    {
        return it->second;
    }

    return std::nullopt;
}

void MetadataHandler::handleMetadata(IMetadataPacket* metadataPacket)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!metadataPacket)
    {
        NX_VERBOSE(this, "%1(): Received null metadata packet; ignoring", __func__);
        return;
    }

    bool handled = false;
    if (const auto eventsPacket = metadataPacket->queryInterface<IEventMetadataPacket>())
    {
        handleEventMetadataPacket(eventsPacket);
        handled = true;
    }

    if (const auto objectsPacket = metadataPacket->queryInterface<IObjectMetadataPacket>())
    {
        handleObjectMetadataPacket(objectsPacket);
        handled = true;
    }

    if (const auto objectTrackBestShotPacket =
        metadataPacket->queryInterface<IObjectTrackBestShotPacket>())
    {
        handleObjectTrackBestShotPacket(objectTrackBestShotPacket);
        handled = true;
    }

    if (!handled)
    {
        NX_VERBOSE(this,
            "WARNING: Received unsupported metadata packet with timestampUs %1; ignoring",
            metadataPacket->timestampUs());
    }
}

void MetadataHandler::handleEventMetadataPacket(
    const Ptr<IEventMetadataPacket>& eventMetadataPacket)
{
    if (eventMetadataPacket->count() <= 0)
    {
        NX_VERBOSE(this, "WARNING: Received empty event packet; ignoring");
        return;
    }

    for (int i = 0; i < eventMetadataPacket->count(); ++i)
    {
        if (const auto eventMetadata = eventMetadataPacket->at(i))
            handleEventMetadata(eventMetadata, eventMetadataPacket->timestampUs());
        else
            break;
    }
}

void MetadataHandler::handleObjectMetadataPacket(
    const Ptr<IObjectMetadataPacket>& objectMetadataPacket)
{
    nx::common::metadata::ObjectMetadataPacket data;
    for (int i = 0; i < objectMetadataPacket->count(); ++i)
    {
        const auto item = objectMetadataPacket->at(i);
        if (!item)
            break;

        nx::common::metadata::ObjectMetadata objectMetadata;
        objectMetadata.typeId = item->typeId();
        objectMetadata.trackId = fromSdkUuidToQnUuid(item->trackId());
        const auto box = item->boundingBox();
        objectMetadata.boundingBox = QRectF(box.x, box.y, box.width, box.height);
        if (m_translateObjectBoundingBoxes)
        {
            objectMetadata.boundingBox =
                m_objectCoordinatesTranslator.translate(objectMetadata.boundingBox);
        }

        NX_VERBOSE(this, "%1(): x %2, y %3, width %4, height %5, typeId %6, id %7",
            __func__, box.x, box.y, box.width, box.height,
            objectMetadata.typeId, objectMetadata.trackId);

        for (int j = 0; j < item->attributeCount(); ++j)
        {
            nx::common::metadata::Attribute attribute;
            attribute.name = QString::fromStdString(item->attribute(j)->name());
            attribute.value = QString::fromStdString(item->attribute(j)->value());
            objectMetadata.attributes.push_back(attribute);

            NX_VERBOSE(this, "%1(): Attribute: %2 %3", __func__, attribute.name, attribute.value);
        }
        data.objectMetadataList.push_back(std::move(objectMetadata));
    }

    if (data.objectMetadataList.empty())
        NX_VERBOSE(this, "%1(): WARNING: ObjectsMetadataPacket is empty", __func__);

    data.timestampUs = objectMetadataPacket->timestampUs();
    data.durationUs = objectMetadataPacket->durationUs();
    data.deviceId = m_resource->getId();

    // This is not very precise, but is ok, since stream index isn't changed very often.
    data.streamIndex = m_resource->analyzedStreamIndex(m_engineId);

    if (data.timestampUs <= 0)
        NX_DEBUG(this, "Invalid ObjectsMetadataPacket timestamp: %1", data.timestampUs);

    pushObjectMetadataToSinks(data);
}

void MetadataHandler::handleObjectTrackBestShotPacket(
    const nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>& objectTrackBestShotPacket)
{
    nx::common::metadata::ObjectMetadataPacket bestShotPacket;
    bestShotPacket.timestampUs = objectTrackBestShotPacket->timestampUs();
    if (bestShotPacket.timestampUs < 0)
    {
        NX_DEBUG(this,
            "Invalid ObjectTrackBestShotPacket timestamp: %1, ignoring packet",
            bestShotPacket.timestampUs);

        return;
    }

    bestShotPacket.durationUs = 0;
    bestShotPacket.deviceId = m_resource->getId();

    // This is not very precise, but is ok, since stream index isn't changed very often.
    bestShotPacket.streamIndex = m_resource->analyzedStreamIndex(m_engineId);

    nx::common::metadata::ObjectMetadata bestShot;
    bestShot.trackId =
        nx::vms_server_plugins::utils::fromSdkUuidToQnUuid(objectTrackBestShotPacket->trackId());
    bestShot.bestShot = true;
    const auto box = objectTrackBestShotPacket->boundingBox();
    bestShot.boundingBox = QRectF(box.x, box.y, box.width, box.height);

    if (m_translateObjectBoundingBoxes)
        bestShot.boundingBox = m_objectCoordinatesTranslator.translate(bestShot.boundingBox);

    bestShotPacket.objectMetadataList.push_back(std::move(bestShot));

    pushObjectMetadataToSinks(bestShotPacket);
}

void MetadataHandler::handleEventMetadata(
    const Ptr<const IEventMetadata>& eventMetadata, qint64 timestampUsec)
{
    auto eventState = nx::vms::api::EventState::undefined;

    const char* const eventTypeId = eventMetadata->typeId();
    // TODO: #dmishin: Properly handle the null value.
    NX_VERBOSE(this, "%1(): typeId %2", __func__, eventTypeId);

    const auto descriptor = eventTypeDescriptor(eventTypeId);
    if (!descriptor)
    {
        NX_DEBUG(this, "Event %1 is not in the list of Events supported by %2 (%3)",
            eventTypeId,
            m_resource->getUserDefinedName(),
            m_resource->getId());
        return;
    }

    if (descriptor->flags.testFlag(nx::vms::api::analytics::EventTypeFlag::stateDependent))
    {
        eventState = eventMetadata->isActive()
            ? nx::vms::api::EventState::active
            : nx::vms::api::EventState::inactive;

        const bool isDuplicate = eventState == nx::vms::api::EventState::inactive
            && lastEventState(eventTypeId) == nx::vms::api::EventState::inactive;

        if (isDuplicate)
        {
            NX_VERBOSE(this, "%1(): Ignoring duplicate event", __func__);
            return;
        }
    }

    setLastEventState(eventTypeId, eventState);

    const auto sdkEvent = nx::vms::event::AnalyticsSdkEventPtr::create(
        m_resource,
        m_engineId,
        eventTypeId,
        eventState,
        eventMetadata->caption(),
        eventMetadata->description(),
        nx::vms::server::sdk_support::attributesMap(eventMetadata),
        timestampUsec);

    if (m_resource->captureEvent(sdkEvent))
    {
        NX_VERBOSE(this, "%1(): Capturing event", __func__);
        return;
    }

    emit sdkEventTriggered(sdkEvent);
}

void MetadataHandler::pushObjectMetadataToSinks(
    const nx::common::metadata::ObjectMetadataPacket& packet)
{
    if (nx::analytics::loggingIni().isLoggingEnabled())
        m_metadataLogger.pushObjectMetadata(packet);

    if (m_metadataSinks.empty())
    {
        NX_DEBUG(this, "No metadata sinks are set for the Engine %1 and the Device %2 (%3)",
            m_engineId, m_resource->getUserDefinedName(), m_resource->getId());
        return;
    }

    if (packet.timestampUs < 0) //< Allow packets with 0 timestmap.
        return;

    const QnCompressedMetadataPtr compressedMetadata =
        nx::common::metadata::toCompressedMetadataPacket(packet);

    if (packet.streamIndex == nx::vms::api::StreamIndex::secondary)
        compressedMetadata->flags |= QnAbstractMediaData::MediaFlags_LowQuality;

    if (!NX_ASSERT(compressedMetadata))
        return;

    bool isOriginalPacketPushed = false;
    for (const MetadataSinkPtr& sink: m_metadataSinks)
    {
        const QnAbstractDataPacketPtr dataPacketToPush = isOriginalPacketPushed
            ? QnAbstractDataPacketPtr(compressedMetadata->clone())
            : compressedMetadata;

        isOriginalPacketPushed = true;
        sink->putData(dataPacketToPush);
    }
}

void MetadataHandler::setMetadataSinks(MetadataSinkSet metadataSinks)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_metadataSinks = std::move(metadataSinks);
}

nx::vms::api::EventState MetadataHandler::lastEventState(const QString& eventTypeId) const
{
    if (m_eventStateMap.contains(eventTypeId))
        return m_eventStateMap[eventTypeId];

    return nx::vms::api::EventState::inactive;
}

void MetadataHandler::setLastEventState(
    const QString& eventTypeId, nx::vms::api::EventState eventState)
{
    m_eventStateMap[eventTypeId] = eventState;
}

} // namespace analytics
} // namespace vms::server
} // namespace nx

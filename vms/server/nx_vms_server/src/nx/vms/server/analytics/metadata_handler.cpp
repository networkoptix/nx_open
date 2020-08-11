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
#include <nx/vms/server/analytics/object_track_best_shot_cache.h>
#include <analytics/common/object_metadata.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <media_server/media_server_module.h>
#include <plugins/vms_server_plugins_ini.h>

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

using ObjectMetadataType = nx::common::metadata::ObjectMetadataType;

template<typename SdkEntityWithAttributes>
void fetchAttributes(
    const Ptr<SdkEntityWithAttributes>& sdkEntity,
    nx::common::metadata::ObjectMetadata* inOutObjectMetadata)
{
    if (!NX_ASSERT(inOutObjectMetadata))
        return;

    for (int i = 0; i < sdkEntity->attributeCount(); ++i)
    {
        nx::common::metadata::Attribute attribute;
        const Ptr<const IAttribute> sdkAttribute = sdkEntity->attribute(i);

        if (!sdkAttribute)
            continue; //< TODO: logging.

        attribute.name = QString::fromStdString(sdkAttribute->name());
        attribute.value = QString::fromStdString(sdkAttribute->value());

        inOutObjectMetadata->attributes.push_back(attribute);
    }
}

MetadataHandler::MetadataHandler(
    QnMediaServerModule* serverModule,
    resource::CameraPtr device,
    QnUuid engineId,
    ViolationHandler violationHandler)
    :
    ServerModuleAware(serverModule),
    m_resource(device),
    m_engineId(engineId),
    m_violationHandler(std::move(violationHandler)),
    m_metadataLogger("outgoing_metadata_", m_resource->getId(), m_engineId),
    m_objectCoordinatesTranslator(device),
    m_objectTrackBestShotProxy(
        serverModule->iFrameSearchHelper(),
        [this](nx::common::metadata::ObjectMetadataPacketPtr objectMetadataPacket)
        {
            pushObjectMetadataToSinks(std::move(objectMetadataPacket));
        },
        std::chrono::seconds(pluginsIni().autoGenerateBestShotDelayS),
        std::chrono::seconds(pluginsIni().trackExpirationDelayS))
{
    const auto engineResource =
        resourcePool()->getResourceById<resource::AnalyticsEngineResource>(m_engineId);

    if (!NX_ASSERT(engineResource))
        return;

    const EngineManifest engineManifest = engineResource->manifest();
    m_translateObjectBoundingBoxes = !engineManifest.capabilities.testFlag(
        EngineManifest::Capability::keepObjectBoundingBoxRotation);

    m_needToAutoGenerateBestShots = !engineManifest.capabilities.testFlag(
        EngineManifest::Capability::noAutoBestShots);

    connect(this,
        &MetadataHandler::sdkEventTriggered,
        serverModule->eventConnector(),
        &event::EventConnector::at_analyticsSdkEvent,
        Qt::QueuedConnection);
}

MetadataHandler::~MetadataHandler()
{
    m_objectTrackBestShotResolver.stop();
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
        NX_VERBOSE(this, "%1(): Received null metadata packet; ignoring.", __func__);
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
        metadataPacket->queryInterface<IObjectTrackBestShotPacket0>())
    {
        handleObjectTrackBestShotPacket(objectTrackBestShotPacket);
        handled = true;
    }

    if (!handled)
    {
        NX_VERBOSE(this,
            "%1(): Received unsupported metadata packet with timestampUs %2; ignoring.", __func__,
            metadataPacket->timestampUs());
    }
}

void MetadataHandler::handleEventMetadataPacket(
    const Ptr<IEventMetadataPacket>& eventMetadataPacket)
{
    if (eventMetadataPacket->count() <= 0)
    {
        NX_VERBOSE(this, "%1(): Received empty event packet; ignoring.", __func__);
        return;
    }

    for (int i = 0; i < eventMetadataPacket->count(); ++i)
    {
        const Ptr<const IEventMetadata> eventMetadata = eventMetadataPacket->at(i);
        if (!eventMetadata)
            break;

        const Ptr<const IEventMetadata0> eventMetadataV0 =
            eventMetadata->queryInterface<IEventMetadata0>();

        if (!eventMetadataV0)
            break;

        handleEventMetadata(eventMetadataV0, eventMetadataPacket->timestampUs());
    }
}

void MetadataHandler::handleObjectMetadataPacket(
    const Ptr<IObjectMetadataPacket>& objectMetadataPacket)
{
    auto data = std::make_shared<nx::common::metadata::ObjectMetadataPacket>() ;
    for (int i = 0; i < objectMetadataPacket->count(); ++i)
    {
        const auto item = objectMetadataPacket->at(i);
        if (!item)
            break;

        nx::common::metadata::ObjectMetadata objectMetadata;
        objectMetadata.analyticsEngineId = m_engineId;
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

        fetchAttributes(item, &objectMetadata);
        data->objectMetadataList.push_back(std::move(objectMetadata));
    }

    if (data->objectMetadataList.empty())
        NX_VERBOSE(this, "%1(): Received empty ObjectsMetadataPacket.", __func__);

    data->timestampUs = objectMetadataPacket->timestampUs();
    data->durationUs = objectMetadataPacket->durationUs();
    data->deviceId = m_resource->getId();

    // This is not very precise, but is ok, since stream index isn't changed very often.
    data->streamIndex = m_resource->analyzedStreamIndex(m_engineId);

    if (data->timestampUs <= 0)
    {
        NX_DEBUG(this, "Received ObjectsMetadataPacket with invalid timestamp: %1",
            data->timestampUs);
    }

    postprocessObjectMetadataPacket(data, ObjectMetadataType::regular);
}

void MetadataHandler::handleObjectTrackBestShotPacket(
    const nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket0>& objectTrackBestShotPacket)
{
    auto bestShotPacket = std::make_shared<nx::common::metadata::ObjectMetadataPacket>();
    bestShotPacket->timestampUs = objectTrackBestShotPacket->timestampUs();
    if (bestShotPacket->timestampUs <= 0)
    {
        NX_DEBUG(this,
            "Received ObjectTrackBestShotPacket with invalid timestamp %1; ignoring the packet.",
            bestShotPacket->timestampUs);

        m_violationHandler(
            {wrappers::ViolationType::invalidMetadataTimestamp, /*violationDetails*/ ""});

        return;
    }

    bestShotPacket->durationUs = 0;
    bestShotPacket->deviceId = m_resource->getId();

    // This is not very precise, but is ok, since stream index isn't changed very often.
    bestShotPacket->streamIndex = m_resource->analyzedStreamIndex(m_engineId);

    nx::common::metadata::ObjectMetadata bestShot;
    bestShot.analyticsEngineId = m_engineId;
    bestShot.trackId =
        nx::vms_server_plugins::utils::fromSdkUuidToQnUuid(objectTrackBestShotPacket->trackId());

    const auto box = objectTrackBestShotPacket->boundingBox();
    if (box.x < 0 || box.y < 0 || box.width < 0 || box.height < 0)
    {
        // The rect is "invalid" - to be treated as unknown.
        bestShot.boundingBox = QRectF(-1, -1, -1, -1); //< Such QRectF value means "unknown".
    }
    else
    {
        bestShot.boundingBox = QRectF(box.x, box.y, box.width, box.height);
        if (m_translateObjectBoundingBoxes)
            bestShot.boundingBox = m_objectCoordinatesTranslator.translate(bestShot.boundingBox);
    }

    const auto sdkBestShotPacketWithImageAndAttributes =
        objectTrackBestShotPacket->queryInterface<
            nx::sdk::analytics::IObjectTrackBestShotPacket>();

    if (sdkBestShotPacketWithImageAndAttributes)
        fetchAttributes(sdkBestShotPacketWithImageAndAttributes, &bestShot);

    bestShotPacket->objectMetadataList.push_back(std::move(bestShot));

    if (sdkBestShotPacketWithImageAndAttributes)
    {
        handleObjectTrackBestShotPacketWithImage(
            bestShot.trackId, bestShotPacket, sdkBestShotPacketWithImageAndAttributes);
    }
    else
    {
        postprocessObjectMetadataPacket(bestShotPacket, ObjectMetadataType::bestShot);
    }
}

void MetadataHandler::handleObjectTrackBestShotPacketWithImage(
    QnUuid trackId,
    const nx::common::metadata::ObjectMetadataPacketPtr& bestShotPacket,
    const Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>& sdkBestShotPacketWithImage)
{
    m_objectTrackBestShotResolver.resolve(
        sdkBestShotPacketWithImage,
        [this, bestShotPacket, trackId](
            std::optional<nx::analytics::db::Image> bestShotImage,
            ObjectMetadataType bestShotType)
        {
            const bool incorrectBestShot =
                (!bestShotImage && bestShotType == ObjectMetadataType::externalBestShot)
                || bestShotType == ObjectMetadataType::undefined;

            if (incorrectBestShot)
            {
                NX_VERBOSE(this, "Unable to fetch Bestshot for Track %1, Device %2, Engine Id %3",
                    trackId, m_resource, m_engineId);
                return;
            }

            if (bestShotImage)
                serverModule()->objectTrackBestShotCache()->insert(trackId, *bestShotImage);

            postprocessObjectMetadataPacket(bestShotPacket, bestShotType);
        });
}

void MetadataHandler::handleEventMetadata(
    const Ptr<const IEventMetadata0>& eventMetadata, qint64 timestampUsec)
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

    const Ptr<const IEventMetadata> eventMetadataWithObjectTrackId =
        eventMetadata->queryInterface<IEventMetadata>();

    QnUuid objectTrackId;
    if (eventMetadataWithObjectTrackId)
        objectTrackId = fromSdkUuidToQnUuid(eventMetadataWithObjectTrackId->trackId());

    const auto sdkEvent = nx::vms::event::AnalyticsSdkEventPtr::create(
        m_resource,
        m_engineId,
        eventTypeId,
        eventState,
        eventMetadata->caption(),
        eventMetadata->description(),
        nx::vms::server::sdk_support::attributesMap(eventMetadata),
        objectTrackId,
        timestampUsec);

    if (m_resource->captureEvent(sdkEvent))
    {
        NX_VERBOSE(this, "%1(): Capturing event", __func__);
        return;
    }

    emit sdkEventTriggered(sdkEvent);
}

void MetadataHandler::postprocessObjectMetadataPacket(
    const nx::common::metadata::ObjectMetadataPacketPtr& packet,
    nx::common::metadata::ObjectMetadataType objectMetadataType)
{
    for (nx::common::metadata::ObjectMetadata& objectMetadata: packet->objectMetadataList)
        objectMetadata.objectMetadataType = objectMetadataType;

    if (m_needToAutoGenerateBestShots)
    {
        if (objectMetadataType == nx::common::metadata::ObjectMetadataType::regular)
            pushObjectMetadataToSinks(packet);

        m_objectTrackBestShotProxy.processObjectMetadataPacket(packet);
    }
    else
    {
        pushObjectMetadataToSinks(packet);
    }
}

void MetadataHandler::pushObjectMetadataToSinks(
    const nx::common::metadata::ObjectMetadataPacketPtr& packet)
{
    if (nx::analytics::loggingIni().isLoggingEnabled())
        m_metadataLogger.pushObjectMetadata(*packet);

    if (m_metadataSinks.empty())
    {
        NX_DEBUG(this, "No metadata sinks are set for the Engine %1 and the Device %2 (%3)",
            m_engineId, m_resource->getUserDefinedName(), m_resource->getId());
        return;
    }

    if (packet->timestampUs < 0) //< Allow packets with 0 timestmap.
        return;

    const QnCompressedMetadataPtr compressedMetadata =
        nx::common::metadata::toCompressedMetadataPacket(*packet);

    if (packet->streamIndex == nx::vms::api::StreamIndex::secondary)
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

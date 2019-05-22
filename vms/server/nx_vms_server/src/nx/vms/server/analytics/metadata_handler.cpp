#include "metadata_handler.h"

#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>

#include <nx/sdk/helpers/ptr.h>
#include <nx/vms_server_plugins/utils/uuid.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/vms/server/event/event_connector.h>
#include <analytics/common/object_detection_metadata.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <media_server/media_server_module.h>

#include <core/resource/camera_resource.h>

#include <nx/analytics/analytics_logging_ini.h>

namespace nx {
namespace vms::server {
namespace analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms::api::analytics;

MetadataHandler::MetadataHandler(
    QnMediaServerModule* serverModule, QnVirtualCameraResourcePtr device, QnUuid engineId):
    ServerModuleAware(serverModule),
    m_resource(device), m_engineId(engineId),
    m_metadataLogger("outgoing_metadata_", m_resource->getId(), m_engineId)
{
    connect(this,
        &MetadataHandler::sdkEventTriggered,
        serverModule->eventConnector(),
        &event::EventConnector::at_analyticsSdkEvent,
        Qt::QueuedConnection);
}

std::optional<EventTypeDescriptor> MetadataHandler::eventTypeDescriptor(
    const QString& eventTypeId) const
{
    if (const auto it = m_eventTypeDescriptors.find(eventTypeId);
        it != m_eventTypeDescriptors.cend())
    {
        return it->second;
    }

    return std::nullopt;
}

void MetadataHandler::handleMetadata(IMetadataPacket* metadataPacket)
{
    if (metadataPacket == nullptr)
    {
        NX_VERBOSE(this, "%1(): Received null metadata packet; ignoring", __func__);
        return;
    }

    bool handled = false;
    if (const auto eventsPacket = queryInterfacePtr<IEventMetadataPacket>(metadataPacket))
    {
        handleEventMetadataPacket(eventsPacket);
        handled = true;
    }

    if (const auto objectsPacket = queryInterfacePtr<IObjectMetadataPacket>(metadataPacket))
    {
        handleObjectMetadataPacket(objectsPacket);
        handled = true;
    }

    if (const auto objectTrackBestShotPacket =
        queryInterfacePtr<IObjectTrackBestShotPacket>(metadataPacket))
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
        if (const auto eventMetadata = toPtr(eventMetadataPacket->at(i)))
            handleEventMetadata(eventMetadata, eventMetadataPacket->timestampUs());
        else
            break;
    }
}

void MetadataHandler::handleObjectMetadataPacket(
    const Ptr<IObjectMetadataPacket>& objectMetadataPacket)
{
    nx::common::metadata::DetectionMetadataPacket data;
    for (int i = 0; i < objectMetadataPacket->count(); ++i)
    {
        const auto item = toPtr(objectMetadataPacket->at(i));
        if (!item)
            break;

        nx::common::metadata::DetectedObject object;
        object.objectTypeId = item->typeId();
        object.objectId = nx::vms_server_plugins::utils::fromSdkUuidToQnUuid(item->id());
        const auto box = item->boundingBox();
        object.boundingBox = QRectF(box.x, box.y, box.width, box.height);

        NX_VERBOSE(this, "%1(): x %2, y %3, width %4, height %5, typeId %6, id %7",
            __func__, box.x, box.y, box.width, box.height, object.objectTypeId, object.objectId);

        for (int i = 0; i < item->attributeCount(); ++i)
        {
            nx::common::metadata::Attribute attribute;
            attribute.name = QString::fromStdString(item->attribute(i)->name());
            attribute.value = QString::fromStdString(item->attribute(i)->value());
            object.labels.push_back(attribute);

            NX_VERBOSE(this, "%1(): Attribute: %2 %3", __func__, attribute.name, attribute.value);
        }
        data.objects.push_back(std::move(object));
    }

    if (data.objects.empty())
        NX_VERBOSE(this, "%1(): WARNING: ObjectsMetadataPacket is empty", __func__);

    data.timestampUsec = objectMetadataPacket->timestampUs();
    data.durationUsec = objectMetadataPacket->durationUs();
    data.deviceId = m_resource->getId();

    if (data.timestampUsec <= 0)
        NX_WARNING(this, "Invalid ObjectsMetadataPacket timestamp: %1", data.timestampUsec);

    if (m_metadataSink && data.timestampUsec >= 0) //< Warn about 0 but still accept it.
        m_metadataSink->putData(nx::common::metadata::toMetadataPacket(data));

    if (m_visualDebugger)
        m_visualDebugger->push(nx::common::metadata::toMetadataPacket(data));

    if (nx::analytics::loggingIni().isLoggingEnabled())
        m_metadataLogger.pushObjectMetadata(data);
}

void MetadataHandler::handleObjectTrackBestShotPacket(
    const nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>& objectTrackBestShotPacket)
{
    nx::common::metadata::DetectionMetadataPacket bestShotPacket;
    bestShotPacket.timestampUsec = objectTrackBestShotPacket->timestampUs();
    if (bestShotPacket.timestampUsec < 0)
    {
        NX_WARNING(this,
            "Invalid ObjectTrackBestShotPacket timestamp: %1, ignoring packet",
            bestShotPacket.timestampUsec);

        return;
    }

    bestShotPacket.durationUsec = 0;
    bestShotPacket.deviceId = m_resource->getId();

    nx::common::metadata::DetectedObject bestShot;
    bestShot.objectId =
        nx::vms_server_plugins::utils::fromSdkUuidToQnUuid(objectTrackBestShotPacket->trackId());
    bestShot.bestShot = true;
    const auto box = objectTrackBestShotPacket->boundingBox();
    bestShot.boundingBox = QRectF(box.x, box.y, box.width, box.height);

    bestShotPacket.objects.push_back(std::move(bestShot));

    if (m_metadataSink)
        m_metadataSink->putData(nx::common::metadata::toMetadataPacket(bestShotPacket));

    if (nx::analytics::loggingIni().isLoggingEnabled())
        m_metadataLogger.pushObjectMetadata(bestShotPacket);
}

void MetadataHandler::handleEventMetadata(
    const Ptr<const IEventMetadata>& eventMetadata, qint64 timestampUsec)
{
    auto eventState = nx::vms::api::EventState::undefined;

    const auto eventTypeId = eventMetadata->typeId();
    NX_VERBOSE(this, "%1(): typeId %2", __func__, eventTypeId);

    auto descriptor = eventTypeDescriptor(eventTypeId);
    if (!descriptor)
    {
        NX_WARNING(this, "Unable to find descriptor for %1", eventTypeId);
        return;
    }

    if (descriptor->flags.testFlag(nx::vms::api::analytics::EventTypeFlag::stateDependent))
    {
        eventState = eventMetadata->isActive() ? nx::vms::api::EventState::active
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

    auto sdkEvent = nx::vms::event::AnalyticsSdkEventPtr::create(m_resource,
        m_engineId,
        eventTypeId,
        eventState,
        eventMetadata->caption(),
        eventMetadata->description(),
        eventMetadata->auxiliaryData(),
        timestampUsec);

    if (m_resource->captureEvent(sdkEvent))
    {
        NX_VERBOSE(this, "%1(): Capturing event", __func__);
        return;
    }

    emit sdkEventTriggered(sdkEvent);
}

void MetadataHandler::setEventTypeDescriptors(DescriptorMap descriptors)
{
    m_eventTypeDescriptors = std::move(descriptors);
}

void MetadataHandler::setMetadataSink(QnAbstractDataReceptor* dataReceptor)
{
    m_metadataSink = dataReceptor;
}

void MetadataHandler::removeMetadataSink(QnAbstractDataReceptor* /*dataReceptor*/)
{
    m_metadataSink = nullptr;
}

void MetadataHandler::setVisualDebugger(
    nx::debugging::AbstractVisualMetadataDebugger* visualDebugger)
{
    m_visualDebugger = visualDebugger;
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

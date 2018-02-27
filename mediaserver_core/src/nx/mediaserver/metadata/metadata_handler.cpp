#include "metadata_handler.h"

#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/mediaserver/event/event_connector.h>
#include <analytics/common/object_detection_metadata.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

MetadataHandler::MetadataHandler()
{
    connect(this, &MetadataHandler::sdkEventTriggered,
        qnEventRuleConnector, &event::EventConnector::at_analyticsSdkEvent,
        Qt::QueuedConnection);
}

nx::api::Analytics::EventType MetadataHandler::eventDescriptor(const QnUuid& eventId) const
{
    for (const auto& descriptor: m_manifest.outputEventTypes)
    {
        if (descriptor.typeId == eventId)
            return descriptor;
    }
    return nx::api::Analytics::EventType();
}

void MetadataHandler::handleMetadata(
    Error error,
    MetadataPacket* metadata)
{
    if (metadata == nullptr)
        return;

    if (error != Error::noError)
        return;

    nxpt::ScopedRef<EventsMetadataPacket> eventsPacket(
        metadata->queryInterface(IID_EventsMetadataPacket));
    if (eventsPacket)
        handleEventsPacket(std::move(eventsPacket));

    nxpt::ScopedRef<ObjectsMetadataPacket> objectsPacket(
        metadata->queryInterface(IID_ObjectsMetadataPacket));
    if (objectsPacket)
        handleObjectsPacket(std::move(objectsPacket));
}

void MetadataHandler::handleEventsPacket(nxpt::ScopedRef<EventsMetadataPacket> packet)
{
    while (true)
    {
        nxpt::ScopedRef<MetadataItem> item(packet->nextItem(), /*increaseRef*/ false);
        if (!item)
            break;

        nxpt::ScopedRef<Event> eventData(item->queryInterface(IID_Event));
        if (eventData)
        {
            const int64_t timestampUsec = packet->timestampUsec();
            handleMetadataEvent(std::move(eventData), timestampUsec);
        }
        else
        {
            NX_VERBOSE(this) << "ERROR: Received event does not implement Event";
        }
    }
}

void MetadataHandler::handleObjectsPacket(nxpt::ScopedRef<ObjectsMetadataPacket> packet)
{
    nx::common::metadata::DetectionMetadataPacket data;
    while (true)
    {
        nxpt::ScopedRef<Object> item(packet->nextItem(), /*increaseRef*/ false);
        if (!item)
            break;
        nx::common::metadata::DetectedObject object;
        object.objectTypeId = nxpt::fromPluginGuidToQnUuid(item->typeId());
        object.objectId = nxpt::fromPluginGuidToQnUuid(item->id());
        const auto box = item->boundingBox();
        object.boundingBox = QRectF(box.x, box.y, box.width, box.height);

        NX_VERBOSE(this) << __func__ << lm("(): x %1, y %2, width %3, height %4, typeId %5, id %6")
            .args(box.x, box.y, box.width, box.height, object.objectTypeId, object.objectId);

        for (int i = 0; i < item->attributeCount(); ++i)
        {
            nx::common::metadata::Attribute attribute;
            attribute.name = QString::fromStdString(item->attribute(i)->name());
            attribute.value = QString::fromStdString(item->attribute(i)->value());
            object.labels.push_back(attribute);

            NX_VERBOSE(this) << __func__ << "Attribute:" << attribute.name << attribute.value;
        }
        data.objects.push_back(std::move(object));
    }
    data.timestampUsec = packet->timestampUsec();
    data.durationUsec = packet->durationUsec();
    data.deviceId = m_resource->getId();

    if (m_dataReceptor)
        m_dataReceptor->putData(nx::common::metadata::toMetadataPacket(data));
}

void MetadataHandler::handleMetadataEvent(
    nxpt::ScopedRef<Event> eventData,
    qint64 timestampUsec)
{
auto eventState = nx::vms::event::EventState::undefined;

        const auto eventTypeId = nxpt::fromPluginGuidToQnUuid(eventData->typeId());
        NX_VERBOSE(this) << __func__ << lm("(): typeId %1").args(eventTypeId);

        auto descriptor = eventDescriptor(eventTypeId);
        if (descriptor.flags.testFlag(nx::api::Analytics::EventTypeFlag::stateDependent))
        {
            eventState = eventData->isActive()
                ? nx::vms::event::EventState::active
                : nx::vms::event::EventState::inactive;

            const bool isDublicate = eventState == nx::vms::event::EventState::inactive
                && lastEventState(eventTypeId) == nx::vms::event::EventState::inactive;

            if (isDublicate)
            {
                NX_VERBOSE(this) << __func__ << lm("(): Ignoring duplicate event");
                return;
            }
        }

    setLastEventState(eventTypeId, eventState);

    auto sdkEvent = nx::vms::event::AnalyticsSdkEventPtr::create(
        m_resource,
        m_manifest.driverId,
        eventTypeId,
        eventState,
        eventData->caption(),
        eventData->description(),
        eventData->auxilaryData(),
        timestampUsec);

    if (m_resource->captureEvent(sdkEvent))
    {
        NX_VERBOSE(this) << __func__ << lm("(): Capturing event");
        return;
    }

    qnEventRuleConnector->at_analyticsSdkEvent(sdkEvent);
}

void MetadataHandler::setResource(const QnSecurityCamResourcePtr& resource)
{
    m_resource = resource;
}

void MetadataHandler::setManifest(const nx::api::AnalyticsDriverManifest& manifest)
{
    m_manifest = manifest;
}

void MetadataHandler::registerDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = dataReceptor;
}

void MetadataHandler::removeDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = nullptr;
}

nx::vms::event::EventState MetadataHandler::lastEventState(const QnUuid& eventId) const
{
    if (m_eventStateMap.contains(eventId))
        return m_eventStateMap[eventId];

    return nx::vms::event::EventState::inactive;
}

void MetadataHandler::setLastEventState(const QnUuid& eventId, nx::vms::event::EventState eventState)
{
    m_eventStateMap[eventId] = eventState;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx

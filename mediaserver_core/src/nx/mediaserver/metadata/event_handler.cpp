#include "event_handler.h"

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

void EventHandler::handleMetadata(
    Error error,
    AbstractMetadataPacket* metadata)
{
    if (metadata == nullptr)
        return;

    if (error != Error::noError)
        return;

    nxpt::ScopedRef<AbstractEventMetadataPacket> eventsPacket(
        (AbstractEventMetadataPacket*)
        metadata->queryInterface(IID_EventMetadataPacket), /*increaseRef*/ false);
    if (eventsPacket)
        handleEventsPacket(std::move(eventsPacket));

    nxpt::ScopedRef<AbstractObjectsMetadataPacket> objectsPacket(
        (AbstractObjectsMetadataPacket*)
        metadata->queryInterface(IID_DetectionMetadataPacket), /*increaseRef*/ false);
    if (objectsPacket)
        handleMetadataPacket(std::move(objectsPacket));
}

void EventHandler::handleEventsPacket(nxpt::ScopedRef<AbstractEventMetadataPacket> packet)
{
    while (true)
    {
        nxpt::ScopedRef<AbstractMetadataItem> item(packet->nextItem(), false);
        if (!item)
            break;

        nxpt::ScopedRef<AbstractDetectedEvent> eventData =
            (AbstractDetectedEvent*)item->queryInterface(IID_DetectedEvent);

        auto timestampUsec = packet->timestampUsec();
        if (eventData)
            handleMetadataEvent(std::move(eventData), timestampUsec);
    }
}

void EventHandler::handleMetadataPacket(nxpt::ScopedRef<AbstractObjectsMetadataPacket> packet)
{
    nx::common::metadata::DetectionMetadataPacket data;
    while (true)
    {
        nxpt::ScopedRef<AbstarctDetectedObject> item(packet->nextItem(), false);
        if (!item)
            break;
        nx::common::metadata::DetectedObject object;
        object.objectTypeId = nxpt::fromPluginGuidToQnUuid(item->eventTypeId());
        object.objectId = nxpt::fromPluginGuidToQnUuid(item->id());
        const auto box = item->boundingBox();
        object.boundingBox = QRectF(box.x, box.y, box.width, box.height);
        for (int i = 0; i < item->attributeCount(); ++i)
        {
            nx::common::metadata::Attribute attribute;
            attribute.name = QString::fromStdString(item->attribute(i)->name);
            attribute.value = QString::fromStdString(item->attribute(i)->value);
            object.labels.push_back(attribute);
        }
        data.objects.push_back(std::move(object));
    }
    data.timestampUsec = packet->timestampUsec();
    data.durationUsec = packet->durationUsec();
    data.deviceId = m_resource->getId();

    if (m_dataReceptor)
        m_dataReceptor->putData(nx::common::metadata::toMetadataPacket(data));
}

void EventHandler::handleMetadataEvent(
    nxpt::ScopedRef<AbstractDetectedEvent> eventData,
    qint64 timestampUsec)
{
    const auto eventState = eventData->isActive()
        ? nx::vms::event::EventState::active
        : nx::vms::event::EventState::inactive;

    const auto eventTypeId = nxpt::fromPluginGuidToQnUuid(eventData->eventTypeId());

    const bool dublicate = eventState == nx::vms::event::EventState::inactive
        && lastEventState(eventTypeId) == nx::vms::event::EventState::inactive;

    if (dublicate)
        return;

    setLastEventState(eventTypeId, eventState);

    auto sdkEvent = nx::vms::event::AnalyticsSdkEventPtr::create(
        m_resource,
        m_pluginId,
        eventTypeId,
        eventState,
        eventData->caption(),
        eventData->description(),
        eventData->auxilaryData(),
        timestampUsec);

    if (m_resource->captureEvent(sdkEvent))
        return;

    qnEventRuleConnector->at_analyticsSdkEvent(sdkEvent);
}

void EventHandler::setResource(const QnSecurityCamResourcePtr& resource)
{
    m_resource = resource;
}

void EventHandler::setPluginId(const QnUuid& pluginId)
{
    m_pluginId = pluginId;
}

void EventHandler::registerDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = dataReceptor;
}

void EventHandler::removeDataReceptor(QnAbstractDataReceptor* dataReceptor)
{
    m_dataReceptor = nullptr;
}

nx::vms::event::EventState EventHandler::lastEventState(const QnUuid& eventId) const
{
    if (m_eventStateMap.contains(eventId))
        return m_eventStateMap[eventId];

    return nx::vms::event::EventState::inactive;
}

void EventHandler::setLastEventState(const QnUuid& eventId, nx::vms::event::EventState eventState)
{
    m_eventStateMap[eventId] = eventState;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx

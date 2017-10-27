#include "event_handler.h"

#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>

#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/mediaserver/event/event_connector.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

void EventHandler::handleMetadata(
    Error error,
    AbstractMetadataPacket* metadata)
{
    nxpt::ScopedRef<AbstractMetadataPacket> metadataPacket(metadata, false);
    if (error != Error::noError)
        return;

    nxpt::ScopedRef<AbstractEventMetadataPacket> packet(
        (AbstractEventMetadataPacket*)
        metadata->queryInterface(IID_EventMetadataPacket), false);

    if (!packet)
        return;

    while (true)
    {
        nxpt::ScopedRef<AbstractMetadataItem> item(
            packet->nextItem(), false);

        if (!item)
            return;

        nxpt::ScopedRef<AbstractDetectedEvent> eventData =
            (AbstractDetectedEvent*) item->queryInterface(IID_DetectedEvent);
        nxpt::ScopedRef<AbstarctDetectedObject> objectData =
            (AbstarctDetectedObject*) item->queryInterface(IID_DetectedObject);

        auto timestampUsec = packet->timestampUsec();
        if (eventData)
            handleMetadataEvent(std::move(eventData), timestampUsec);
        else if (objectData)
            handleMetadataObject(std::move(objectData), timestampUsec);
    }
}

void EventHandler::handleMetadataObject(
    nxpt::ScopedRef<nx::sdk::metadata::AbstarctDetectedObject> eventData,
    qint64 timestampUsec)
{
    QnAbstractMediaDataPtr metadata(new QnCompressedMetadata(MetadataType::ObjectDetection));
    //metadata->m_duration = ? ;
    //metadata->m_data = ? ;

    if (m_dataReceptor)
        m_dataReceptor->putData(metadata);
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

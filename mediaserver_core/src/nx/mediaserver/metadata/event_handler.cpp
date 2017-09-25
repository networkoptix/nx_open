#include "event_handler.h"

#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>

#include <nx/sdk/metadata/abstract_event_metadata_packet.h>
#include <nx/sdk/metadata/abstract_detection_metadata_packet.h>

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

    nxpt::ScopedRef<AbstractEventMetadataPacket> eventPacket(
        (AbstractEventMetadataPacket*)
        metadata->queryInterface(IID_EventMetadataPacket), false);

    if (!eventPacket)
        return;

    while(true)
    {
        nxpt::ScopedRef<AbstractDetectedEvent> eventData(
            eventPacket->nextItem(), false);

        if (!eventData)
            return;

        const auto eventState = eventData->isActive()
            ? nx::vms::event::EventState::active
            : nx::vms::event::EventState::inactive;

        const auto eventTypeId = nxpt::fromPluginGuidToQnUuid(eventData->eventTypeId());

        const bool dublicate = eventState == nx::vms::event::EventState::inactive
            && lastEventState(eventTypeId) == nx::vms::event::EventState::inactive;

        if (dublicate)
            continue;

        setLastEventState(eventTypeId, eventState);

        qnEventRuleConnector->at_analyticsSdkEvent(
            m_resource,
            m_pluginId,
            eventTypeId,
            eventState,
            eventData->caption(),
            eventData->description(),
            eventPacket->timestampUsec());
    }
}

void EventHandler::setResource(const QnResourcePtr& resource)
{
    m_resource = resource;
}

void EventHandler::setPluginId(const QnUuid& pluginId)
{
    m_pluginId = pluginId;
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

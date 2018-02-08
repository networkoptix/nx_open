#include "event_handler.h"

#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>

#include <nx/sdk/metadata/events_metadata_packet.h>
#include <nx/sdk/metadata/objects_metadata_packet.h>
#include <nx/vms/event/events/events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <core/resource/security_cam_resource.h>
#include <nx/mediaserver/event/event_connector.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

nx::api::Analytics::EventType EventHandler::eventDescriptor(const QnUuid& eventId) const
{
    for (const auto& descriptor: m_manifest.outputEventTypes)
    {
        if (descriptor.eventTypeId == eventId)
            return descriptor;
    }
    return nx::api::Analytics::EventType();
}

void EventHandler::handleMetadata(
    Error error,
    MetadataPacket* metadata)
{
    nxpt::ScopedRef<MetadataPacket> metadataPacket(metadata, false);
    if (error != Error::noError)
        return;

    nxpt::ScopedRef<EventsMetadataPacket> eventPacket(
        (EventsMetadataPacket*)
        metadata->queryInterface(IID_EventsMetadataPacket), false);

    if (!eventPacket)
        return;

    while(true)
    {
        nxpt::ScopedRef<Event> eventData(
            eventPacket->nextItem(), false);

        if (!eventData)
            return;

        auto eventState = nx::vms::event::EventState::undefined;

        const auto eventTypeId = nxpt::fromPluginGuidToQnUuid(eventData->eventTypeId());

        auto descriptor = eventDescriptor(eventTypeId);
        if (descriptor.flags.testFlag(nx::api::Analytics::EventTypeFlag::stateDependent))
        {
            eventState = eventData->isActive()
                ? nx::vms::event::EventState::active
                : nx::vms::event::EventState::inactive;

            const bool isDublicate = eventState == nx::vms::event::EventState::inactive
                && lastEventState(eventTypeId) == nx::vms::event::EventState::inactive;

            if (isDublicate)
                continue;
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
            eventPacket->timestampUsec());

        if (m_resource->captureEvent(sdkEvent))
            continue;

        qnEventRuleConnector->at_analyticsSdkEvent(sdkEvent);
    }
}

void EventHandler::setResource(const QnSecurityCamResourcePtr& resource)
{
    m_resource = resource;
}

void EventHandler::setManifest(const nx::api::AnalyticsDriverManifest& manifest)
{
    m_manifest = manifest;
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

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
    // TODO: #dmishin check reference counting here;
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

        auto eventState = eventData->isActive()
            ? nx::vms::event::EventState::active
            : nx::vms::event::EventState::inactive;

        nxpl::NX_GUID kMd = {{0xF1, 0xF7, 0x23, 0xBB, 0x20, 0xC8, 0x45, 0x3A, 0xAF, 0xCE, 0x86, 0xF3, 0x18, 0xCB, 0xF0, 0x97}};
        if (eventData->eventTypeId() == kMd)
            qDebug() << "HANDLING MOTION DETECTION!" << eventData->isActive();

        qnEventRuleConnector->at_analyticsSdkEvent(
            m_resource,
            m_pluginId,
            nxpt::fromPluginGuidToQnUuid(eventData->eventTypeId()),
            eventState,
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

} // namespace metadata
} // namespace mediaserver
} // namespace nx
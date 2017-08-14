#include "event_handler.h"

#include <plugins/plugin_tools.h>

#include <plugins/metadata/abstract_event_metadata_packet.h>
#include <plugins/metadata/abstract_detection_metadata_packet.h>

#include <nx/mediaserver/event/event_connector.h>

#ifdef Q_OS_WIN
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

namespace nx {
namespace mediaserver {
namespace metadata {

namespace metadata_sdk = nx::sdk::metadata; 

void EventHandler::handleMetadata(
    metadata_sdk::Error error,
    metadata_sdk::AbstractMetadataPacket* metadata)
{
    // TODO: #dmishin check reference counting here;
    nxpt::ScopedRef<metadata_sdk::AbstractMetadataPacket> metadataPacket(metadata);
    if (error != metadata_sdk::Error::noError)
        return;

    nxpt::ScopedRef<metadata_sdk::AbstractEventMetadataPacket> eventPacket(
        (metadata_sdk::AbstractEventMetadataPacket*)
        metadata->queryInterface(metadata_sdk::IID_EventMetadataPacket));

    if (!eventPacket)
        return;

    while(true)
    {
        nxpt::ScopedRef<metadata_sdk::AbstractDetectedEvent> eventData(
            eventPacket->nextItem());

        if (!eventData)
            return;

        auto eventState = eventData->isActive()
            ? nx::vms::event::EventState::active
            : nx::vms::event::EventState::inactive;

        qnEventRuleConnector->at_analyticsSdkEvent(
            m_resource,
            m_pluginId,
            fromPluginGuidToQnUuid(eventData->eventTypeId()),
            eventState,
            eventPacket->timestampUsec());
    }
}

QnUuid EventHandler::fromPluginGuidToQnUuid(nxpl::NX_GUID& guid)
{
    return QnUuid(QUuid(
        ntohl(*(unsigned int*)guid.bytes),
        ntohs(*(unsigned short*)(guid.bytes + 4)),
        ntohs(*(unsigned short*)(guid.bytes + 6)),
        guid.bytes[8],
        guid.bytes[9],
        guid.bytes[10],
        guid.bytes[11],
        guid.bytes[12],
        guid.bytes[13],
        guid.bytes[14],
        guid.bytes[15]));
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
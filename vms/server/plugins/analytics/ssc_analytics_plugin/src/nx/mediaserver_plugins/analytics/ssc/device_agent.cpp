#include "device_agent.h"

#include <QtCore/QString>

#include <nx/fusion/model_functions.h>

#include <nx/sdk/common/string.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/sdk/analytics/common_event.h>
#include <nx/sdk/analytics/common_event_metadata_packet.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include "log.h"

#define NX_URL_PRINT NX_PRINT << m_url.host().toStdString() << " : "

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace ssc {

namespace {

nx::sdk::analytics::CommonEventMetadataPacket* createCommonEventMetadataPacket(
    const EventType& eventType, int logicalId)
{
    using namespace std::chrono;

    auto packet = new nx::sdk::analytics::CommonEventMetadataPacket();
    auto commonEvent = new nx::sdk::analytics::CommonEvent();
    commonEvent->setTypeId(eventType.id.toStdString());
    commonEvent->setDescription(eventType.name.toStdString());
    commonEvent->setAuxiliaryData(std::to_string(logicalId));

    packet->addEvent(commonEvent);
    packet->setTimestampUsec(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUsec(-1);
    return packet;
}

} // namespace

DeviceAgent::DeviceAgent(Engine* plugin,
    const nx::sdk::DeviceInfo& deviceInfo,
    const EngineManifest& typedManifest)
    :
    m_engine(plugin),
    m_url(deviceInfo.url),
    m_cameraLogicalId(deviceInfo.logicalId)
{
    nx::vms::api::analytics::DeviceAgentManifest typedDeviceAgentManifest;
    for (const auto& eventType: typedManifest.eventTypes)
        typedDeviceAgentManifest.supportedEventTypeIds.push_back(eventType.id);
    m_deviceAgentManifest = QJson::serialized(typedDeviceAgentManifest);

    NX_URL_PRINT << "SSC DeviceAgent created for device " << deviceInfo.model;
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    NX_URL_PRINT << "SSC DeviceAgent destroyed";
}

void* DeviceAgent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::analytics::IID_DeviceAgent)
    {
        addRef();
        return static_cast<DeviceAgent*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void DeviceAgent::sendEventPacket(const EventType& event) const
{
    ++m_packetId;
    auto packet = createCommonEventMetadataPacket(event, m_cameraLogicalId);
    m_handler->handleMetadata(packet);
    NX_URL_PRINT << "Event [pulse] packetId = " << m_packetId << " "
        << event.internalName.toUtf8().constData() << " sent to server";
}

nx::sdk::Error DeviceAgent::startFetchingMetadata(
    const nx::sdk::analytics::IMetadataTypes* /*metadataTypes*/)
{
    m_engine->registerCamera(m_cameraLogicalId, this);
    return nx::sdk::Error::noError;
}

nx::sdk::Error DeviceAgent::stopFetchingMetadata()
{
    m_engine->unregisterCamera(m_cameraLogicalId);
    return nx::sdk::Error::noError;
}

const nx::sdk::IString* DeviceAgent::manifest(nx::sdk::Error* error) const
{
    *error = nx::sdk::Error::noError;
    return new nx::sdk::common::String(m_deviceAgentManifest);
}

sdk::Error DeviceAgent::setHandler(nx::sdk::analytics::DeviceAgent::IHandler* handler)
{
    m_handler = handler;
    return sdk::Error::noError;
}

sdk::Error DeviceAgent::setNeededMetadataTypes(
    const sdk::analytics::IMetadataTypes* metadataTypes)
{
    if (metadataTypes->eventTypeIds()->count() == 0)
    {
        stopFetchingMetadata();
        return sdk::Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

void DeviceAgent::setSettings(const nx::sdk::Settings* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::Settings* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

} // namespace ssc
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

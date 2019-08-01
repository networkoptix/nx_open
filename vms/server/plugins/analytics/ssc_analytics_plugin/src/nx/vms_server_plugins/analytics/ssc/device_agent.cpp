#include "device_agent.h"

#include <QtCore/QString>

#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>

#include "log.h"

#define NX_URL_PRINT NX_PRINT << m_url.host().toStdString() << " : "

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace ssc {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

EventMetadataPacket* createCommonEventMetadataPacket(
    const EventType& eventType, int /*logicalId*/)
{
    using namespace std::chrono;

    auto packet = new EventMetadataPacket();
    auto eventMetadata = makePtr<nx::sdk::analytics::EventMetadata>();
    eventMetadata->setTypeId(eventType.id.toStdString());
    eventMetadata->setDescription(eventType.name.toStdString());
    packet->addItem(eventMetadata.get());
    packet->setTimestampUs(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUs(-1);
    return packet;
}

} // namespace

DeviceAgent::DeviceAgent(Engine* plugin,
    const IDeviceInfo* deviceInfo,
    const EngineManifest& typedManifest)
    :
    m_engine(plugin),
    m_url(deviceInfo->url()),
    m_cameraLogicalId(QString(deviceInfo->logicalId()).toInt())
{
    nx::vms::api::analytics::DeviceAgentManifest typedDeviceAgentManifest;
    for (const auto& eventType: typedManifest.eventTypes)
        typedDeviceAgentManifest.supportedEventTypeIds.push_back(eventType.id);
    m_deviceAgentManifest = QJson::serialized(typedDeviceAgentManifest);

    NX_URL_PRINT << "SSC DeviceAgent created for device " << deviceInfo->model();
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    NX_URL_PRINT << "SSC DeviceAgent destroyed";
}

void DeviceAgent::sendEventPacket(const EventType& event) const
{
    ++m_packetId;
    auto packet = createCommonEventMetadataPacket(event, m_cameraLogicalId);
    m_handler->handleMetadata(packet);
    NX_URL_PRINT << "Event [pulse] packetId = " << m_packetId << " "
        << event.internalName.toUtf8().constData() << " sent to server";
}

void DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/)
{
    m_engine->registerCamera(m_cameraLogicalId, this);
}

void DeviceAgent::stopFetchingMetadata()
{
    m_engine->unregisterCamera(m_cameraLogicalId);
}

StringResult DeviceAgent::manifest() const
{
    return new nx::sdk::String(m_deviceAgentManifest);
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

Result<void> DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    const auto eventTypeIds = metadataTypes->eventTypeIds();
    if (const char* const kMessage = "Event type id list is nullptr";
        !NX_ASSERT(eventTypeIds, kMessage))
    {
        return error(ErrorCode::internalError, kMessage);
    }

    if (eventTypeIds->count() == 0)
        stopFetchingMetadata();

    startFetchingMetadata(metadataTypes);
    return {};
}

StringMapResult DeviceAgent::setSettings(const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
    return nullptr;
}

SettingsResponseResult DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

} // namespace ssc
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

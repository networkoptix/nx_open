#include <chrono>

#include <nx/sdk/analytics/common/event.h>
#include <nx/sdk/analytics/common/event_metadata_packet.h>
#include <nx/vms_server_plugins/utils/uuid.h>
#include <nx/utils/log/log_main.h>
#include <nx/fusion/model_functions.h>

#include <nx/sdk/common/string.h>

#include "common.h"
#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(
    Engine* engine,
    const DeviceInfo& deviceInfo,
    const nx::vms::api::analytics::DeviceAgentManifest& deviceAgentParsedManifest)
    :
    m_engine(engine),
    m_jsonManifest(QJson::serialized(deviceAgentParsedManifest)),
    m_parsedManifest(deviceAgentParsedManifest)

{
    setDeviceInfo(deviceInfo);
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
}

void* DeviceAgent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_DeviceAgent)
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

void DeviceAgent::setSettings(const nx::sdk::IStringMap* /*settings*/)
{
    // This plugin doesn't use DeviceAgent setting, it just ignores them.
}

nx::sdk::IStringMap* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

nx::sdk::Error DeviceAgent::setHandler(nx::sdk::analytics::IDeviceAgent::IHandler* handler)
{
    m_handler = handler;
    return nx::sdk::Error::noError;
}

nx::sdk::Error DeviceAgent::setNeededMetadataTypes(
    const nx::sdk::analytics::IMetadataTypes* metadataTypes)
{
    if (metadataTypes->isEmpty())
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

nx::sdk::Error DeviceAgent::startFetchingMetadata(
    const nx::sdk::analytics::IMetadataTypes* metadataTypes)
{
    auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto packet = new nx::sdk::analytics::common::EventMetadataPacket();

            for (const auto& dahuaEvent: events)
            {
                if (dahuaEvent.channel.has_value() && dahuaEvent.channel != m_channel)
                    return;

                auto event = new nx::sdk::analytics::common::Event();
                NX_VERBOSE(this, "Got event: %1 %2 Channel %3",
                    dahuaEvent.caption, dahuaEvent.description, m_channel);

                event->setTypeId(dahuaEvent.typeId.toStdString());
                event->setCaption(dahuaEvent.caption.toStdString());
                event->setDescription(dahuaEvent.description.toStdString());
                event->setIsActive(dahuaEvent.isActive);
                event->setConfidence(1.0);
                //event->setAuxiliaryData(dahuaEvent.fullEventName.toStdString());

                packet->setTimestampUs(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUs(-1);
                packet->addItem(event);
            }

            m_handler->handleMetadata(packet);
        };

    NX_ASSERT(m_engine);
    std::vector<QString> eventTypes;

    for (int i = 0; i < metadataTypes->eventTypeIds()->count(); ++i)
        eventTypes.push_back(metadataTypes->eventTypeIds()->at(i));

    m_monitor = std::make_unique<MetadataMonitor>(
        m_engine->parsedManifest(),
        m_parsedManifest,
        m_url,
        m_auth,
        eventTypes);

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return Error::noError;
}

void DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);

    m_monitor = nullptr;
}

const IString* DeviceAgent::manifest(Error* error) const
{
    if (m_jsonManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return new nx::sdk::common::String(m_jsonManifest);
}

void DeviceAgent::setDeviceInfo(const nx::sdk::DeviceInfo& deviceInfo)
{
    m_url = deviceInfo.url;
    m_model = deviceInfo.model;
    m_firmware = deviceInfo.firmware;
    m_auth.setUser(deviceInfo.login);
    m_auth.setPassword(deviceInfo.password);
    m_uniqueId = deviceInfo.uid;
    m_sharedId = deviceInfo.sharedId;
    m_channel = deviceInfo.channel;
}

} // namespace nx::vms_server_plugins::analytics::dahua

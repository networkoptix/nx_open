#include <chrono>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/utils/log/log_main.h>
#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>

#include "common.h"
#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(
    Engine* engine,
    const IDeviceInfo* deviceInfo,
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

void DeviceAgent::setSettings(const IStringMap* /*settings*/)
{
    // This plugin doesn't use DeviceAgent setting, it just ignores them.
}

IStringMap* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

Error DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

Error DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->isEmpty())
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

Error DeviceAgent::startFetchingMetadata(const IMetadataTypes* metadataTypes)
{
    auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto packet = new EventMetadataPacket();

            for (const auto& dahuaEvent: events)
            {
                if (dahuaEvent.channel.has_value() && dahuaEvent.channel != m_channelNumber)
                    return;

                auto eventMetadata = new nx::sdk::analytics::EventMetadata();
                NX_VERBOSE(this, "Got event: %1 %2 Channel %3",
                    dahuaEvent.caption, dahuaEvent.description, m_channelNumber);

                eventMetadata->setTypeId(dahuaEvent.typeId.toStdString());
                eventMetadata->setCaption(dahuaEvent.caption.toStdString());
                eventMetadata->setDescription(dahuaEvent.description.toStdString());
                eventMetadata->setIsActive(dahuaEvent.isActive);
                eventMetadata->setConfidence(1.0);
                //eventMetadata->setAuxiliaryData(dahuaEvent.fullEventName.toStdString());

                packet->setTimestampUs(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUs(-1);
                packet->addItem(eventMetadata);
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
    return new nx::sdk::String(m_jsonManifest);
}

void DeviceAgent::setDeviceInfo(const IDeviceInfo* deviceInfo)
{
    m_url = deviceInfo->url();
    m_model = deviceInfo->model();
    m_firmware = deviceInfo->firmware();
    m_auth.setUser(deviceInfo->login());
    m_auth.setPassword(deviceInfo->password());
    m_uniqueId = deviceInfo->id();
    m_sharedId = deviceInfo->sharedId();
    m_channelNumber = deviceInfo->channelNumber();
}

} // namespace nx::vms_server_plugins::analytics::dahua

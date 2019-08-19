#include <chrono>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/utils/log/log_main.h>
#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

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

void DeviceAgent::doSetSettings(
    Result<const IStringMap*>* /*outResult*/, const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* /*outResult*/) const
{
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    *outResult = Result<void>();
    if (neededMetadataTypes->isEmpty())
        stopFetchingMetadata();
    else
        *outResult = startFetchingMetadata(neededMetadataTypes);
}

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* metadataTypes)
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

                auto eventMetadata = makePtr<EventMetadata>();
                NX_VERBOSE(this, "Got event: %1 %2 Channel %3",
                    dahuaEvent.caption, dahuaEvent.description, m_channelNumber);

                eventMetadata->setTypeId(dahuaEvent.typeId.toStdString());
                eventMetadata->setCaption(dahuaEvent.caption.toStdString());
                eventMetadata->setDescription(dahuaEvent.description.toStdString());
                eventMetadata->setIsActive(dahuaEvent.isActive);
                eventMetadata->setConfidence(1.0);

                packet->setTimestampUs(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUs(-1);
                packet->addItem(eventMetadata.get());
            }

            m_handler->handleMetadata(packet);
        };

    NX_ASSERT(m_engine);
    std::vector<QString> eventTypes;

    const auto eventTypeIds = metadataTypes->eventTypeIds();
    if (const char* const kMessage = "Event type id list is empty";
        !NX_ASSERT(eventTypeIds, kMessage))
    {
        return error(ErrorCode::internalError, kMessage);
    }

    for (int i = 0; i < eventTypeIds->count(); ++i)
        eventTypes.push_back(eventTypeIds->at(i));

    m_monitor = std::make_unique<MetadataMonitor>(
        m_engine->parsedManifest(),
        m_parsedManifest,
        m_url,
        m_auth,
        eventTypes);

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return {};
}

void DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);

    m_monitor = nullptr;
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    if (m_jsonManifest.isEmpty())
        *outResult = error(ErrorCode::otherError, "DeviceAgent manifest is empty");
    else
        *outResult = new nx::sdk::String(m_jsonManifest);
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

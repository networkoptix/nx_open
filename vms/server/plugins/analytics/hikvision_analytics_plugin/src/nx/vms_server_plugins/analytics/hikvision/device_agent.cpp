#include "device_agent.h"

#include "common.h"

#include <chrono>

#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/helpers/error.h>

#include <nx/utils/log/log_main.h>
#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine):
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
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

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

Result<void> DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->isEmpty())
        stopFetchingMetadata();

    return startFetchingMetadata(metadataTypes);
}

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* metadataTypes)
{
    auto monitorHandler =
        [this](const HikvisionEventList& events)
        {
            using namespace std::chrono;
            auto packet = new EventMetadataPacket();

            for (const auto& hikvisionEvent: events)
            {
                const bool wrongChannel = hikvisionEvent.channel.is_initialized()
                    && hikvisionEvent.channel != m_channelNumber;

                if (wrongChannel)
                    return;

                auto eventMetadata = makePtr<EventMetadata>();
                NX_VERBOSE(this, lm("Got event: %1 %2 Channel %3")
                    .args(hikvisionEvent.caption, hikvisionEvent.description, m_channelNumber));

                eventMetadata->setTypeId(hikvisionEvent.typeId.toStdString());
                eventMetadata->setCaption(hikvisionEvent.caption.toStdString());
                eventMetadata->setDescription(hikvisionEvent.description.toStdString());
                eventMetadata->setIsActive(hikvisionEvent.isActive);
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

    const auto eventTypeIdList = metadataTypes->eventTypeIds();
    if (const char* const  kMessage = "Event type id list is nullptr";
        !NX_ASSERT(eventTypeIdList, kMessage))
    {
        return error(ErrorCode::internalError, kMessage);
    }

    for (int i = 0; i < eventTypeIdList->count(); ++i)
        eventTypes.push_back(eventTypeIdList->at(i));

    m_monitor =
        std::make_unique<HikvisionMetadataMonitor>(
            m_engine->engineManifest(),
            QJson::deserialized<nx::vms::api::analytics::DeviceAgentManifest>(
                m_deviceAgentManifest),
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

StringResult DeviceAgent::manifest() const
{
    if (m_deviceAgentManifest.isEmpty())
        return error(ErrorCode::otherError, "DeviceAgent manifest is empty");

    return new nx::sdk::String(m_deviceAgentManifest);
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

void DeviceAgent::setDeviceAgentManifest(const QByteArray& manifest)
{
    m_deviceAgentManifest = manifest;
}

void DeviceAgent::setEngineManifest(const Hikvision::EngineManifest& manifest)
{
    m_engineManifest = manifest;
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

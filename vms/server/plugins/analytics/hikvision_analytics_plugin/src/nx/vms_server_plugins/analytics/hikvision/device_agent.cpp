#include "device_agent.h"

#include "common.h"

#include <chrono>

#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>
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
    m_engine(engine),
    m_metadataParser(engine->plugin()->utilityProvider())
{
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
}

void DeviceAgent::doSetSettings(
    Result<const ISettingsResponse*>* /*outResult*/, const IStringMap* /*settings*/)
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

    m_metadataParser.setHandler(m_handler);
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    stopFetchingMetadata();
    *outResult = startFetchingMetadata(neededMetadataTypes);
}

void DeviceAgent::doPushDataPacket(Result<void>* /*outResult*/, IDataPacket* dataPacket)
{
    const auto metadataPacket = dataPacket->queryInterface<ICustomMetadataPacket>();
    QByteArray metadataBytes(metadataPacket->data(), metadataPacket->dataSize());

    std::unique_lock lock(m_metadataParserMutex);
    m_metadataParser.parsePacket(metadataBytes, metadataPacket->timestampUs());
}

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* metadataTypes)
{
    auto monitorHandler =
        [this](const HikvisionEventList& events)
        {
            if (!NX_ASSERT(m_handler))
                return;


            for (auto hikvisionEvent: events)
            {
                const bool wrongChannel = hikvisionEvent.channel.has_value()
                    && hikvisionEvent.channel != m_channelNumber;

                if (wrongChannel)
                    return;

                {
                    std::unique_lock lock(m_metadataParserMutex);
                    m_metadataParser.processEvent(&hikvisionEvent);
                }

                auto eventMetadata = makePtr<EventMetadata>();
                NX_VERBOSE(this, lm("Got event: %1 %2 Channel %3")
                    .args(hikvisionEvent.caption, hikvisionEvent.description, m_channelNumber));

                eventMetadata->setTypeId(hikvisionEvent.typeId.toStdString());
                eventMetadata->setTrackId(hikvisionEvent.trackId);
                eventMetadata->setCaption(hikvisionEvent.caption.toStdString());
                eventMetadata->setDescription(hikvisionEvent.description.toStdString());
                eventMetadata->setIsActive(hikvisionEvent.isActive);
                eventMetadata->setConfidence(1.0);

                auto packet = makePtr<EventMetadataPacket>();
                packet->setTimestampUs(hikvisionEvent.dateTime.toMSecsSinceEpoch() * 1000);
                packet->addItem(eventMetadata.get());
                m_handler->handleMetadata(packet.get());
            }
        };

    NX_ASSERT(m_engine);
    std::vector<QString> eventTypes;
    std::vector<QString> objectTypes;

    const auto eventTypeIdList = metadataTypes->eventTypeIds();
    if (const char* const kMessage = "Event type id list is null";
        !NX_ASSERT(eventTypeIdList, kMessage))
    {
        return error(ErrorCode::internalError, kMessage);
    }

    for (int i = 0; i < eventTypeIdList->count(); ++i)
        eventTypes.push_back(eventTypeIdList->at(i));

    const auto objectTypeIdList = metadataTypes->objectTypeIds();
    if (const char* const kMessage = "Object type id list is null";
        !NX_ASSERT(objectTypeIdList, kMessage))
    {
        return error(ErrorCode::internalError, kMessage);
    }

    for (int i = 0; i < objectTypeIdList->count(); ++i)
        objectTypes.push_back(objectTypeIdList->at(i));

    m_monitor =
        std::make_unique<HikvisionMetadataMonitor>(
            m_engine->engineManifest(),
            QJson::deserialized<nx::vms::api::analytics::DeviceAgentManifest>(
                m_deviceAgentManifest),
            m_url,
            m_auth,
            eventTypes,
            objectTypes);

    m_monitor->setDeviceInfo(m_name, m_uniqueId);
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
    if (m_deviceAgentManifest.isEmpty())
        *outResult = error(ErrorCode::otherError, "DeviceAgent manifest is empty");
    else
        *outResult = new nx::sdk::String(m_deviceAgentManifest);
}

void DeviceAgent::setDeviceInfo(const IDeviceInfo* deviceInfo)
{
    m_url = deviceInfo->url();
    m_model = deviceInfo->model();
    m_firmware = deviceInfo->firmware();
    m_name = deviceInfo->name();
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

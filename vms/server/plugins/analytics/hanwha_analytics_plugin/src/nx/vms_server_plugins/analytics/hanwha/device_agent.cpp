#include "device_agent.h"

#include <chrono>
#include <sstream>
#include <charconv>
#include <string>
#include <vector>

#include <QtCore/QUrl>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/timestamped_object_metadata.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>

#include <nx/utils/log/log.h>
#include <nx/vms/server/analytics/predefined_attributes.h>

#include <nx/sdk/helpers/string_map.h>

#include <plugins/resource/hanwha/hanwha_request_helper.h>

#include "common.h"
#include "device_response_json_parser.h"

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

//-------------------------------------------------------------------------------------------------

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo,
    bool isNvr, QSize maxResolution)
    :
    m_engine(engine),
    m_frameSize(maxResolution.width(), maxResolution.height()),
    m_settingsProcessor(
        m_settings,
        DeviceAccessInfo{ deviceInfo->url(), deviceInfo->login(), deviceInfo->password() },
        deviceInfo->channelNumber(),
        isNvr,
        m_frameSize),
    m_objectMetadataXmlParser(m_engine->manifest(), m_engine->objectMetadataAttributeFilters())
{
    this->setDeviceInfo(deviceInfo);
}

//-------------------------------------------------------------------------------------------------

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    m_engine->deviceAgentIsAboutToBeDestroyed(m_sharedId);
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    const auto eventTypeIds = neededMetadataTypes->eventTypeIds();
    if (const char* const kMessage = "Event type id list is null";
        !NX_ASSERT(eventTypeIds, kMessage))
    {
        *outResult = error(ErrorCode::internalError, kMessage);
        return;
    }

    stopFetchingMetadata();

    if (eventTypeIds->count() != 0)
        *outResult = startFetchingMetadata(neededMetadataTypes);
}

//-------------------------------------------------------------------------------------------------

/**
 * Receives `dataPacket` from Server. This packet is a `CustomMetadataPacket`, that contains xml
 * with the information about events occurred. The function parses the xml and extracts
 * the information about objects. Then it creates `objectMetadataPacket, attaches `objectMetadata`
 * (with object information) to it and sends to server using `m_handler`.
*/
void DeviceAgent::doPushDataPacket(Result<void>* /*outResult*/, IDataPacket* dataPacket)
{
    const auto incomingPacket = dataPacket->queryInterface<ICustomMetadataPacket>();
    QByteArray xmlData(incomingPacket->data(), incomingPacket->dataSize());

    const auto ts = incomingPacket->timestampUs();

    auto [outEventPacket, outObjectPacket] = m_objectMetadataXmlParser.parse(xmlData);

    if (outEventPacket && outEventPacket->count())
    {
        outEventPacket->setTimestampUs(ts);
        outEventPacket->setDurationUs(-1);

        if (NX_ASSERT(m_handler))
            m_handler->handleMetadata(outEventPacket.get());
    }

    if (outObjectPacket && outObjectPacket->count())
    {
        outObjectPacket->setTimestampUs(ts);
        outObjectPacket->setDurationUs(1'000'000); //< 1 second

        if (NX_ASSERT(m_handler))
            m_handler->handleMetadata(outObjectPacket.get());
    }
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::setSupportedEventCategoties()
{
    m_settings.analyticsCategories.fill(false);

    m_settings.analyticsCategories[motionDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.MotionDetection");

    m_settings.analyticsCategories[shockDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.ShockDetection");

    m_settings.analyticsCategories[tamperingDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.Tampering");

    m_settings.analyticsCategories[defocusDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.DefocusDetection");

    m_settings.analyticsCategories[fogDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.FogDetection");

    m_settings.analyticsCategories[videoAnalytics] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Passing")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Entering")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Exiting")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.AppearDisappear")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Intrusion")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Loitering");

    m_settings.analyticsCategories[objectDetection] =
        !m_manifest.supportedObjectTypeIds.isEmpty();

    m_settings.analyticsCategories[audioDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioDetection");

    m_settings.analyticsCategories[audioAnalytics] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.Scream")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.Gunshot")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.Explosion")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.GlassBreak");
}

//-------------------------------------------------------------------------------------------------

/**
 * Read the plugin settings from the server to deviceAgent settings and write them into the
 * agent's device (usually a camera).
 * If a setting can not be written, its name is added to `outResult` error map.
 * if a setting is written successfully, corresponding deviceAgest setting is updated.
*/
/*virtual*/ void DeviceAgent::doSetSettings(
    Result<const IStringMap*>* outResult, const IStringMap* sourceMap) /*override*/
{
    if (!m_serverHasSentInitialSettings)
    {
        // we should ignore initial settings
        m_serverHasSentInitialSettings = true;
        return;
    }

    const int settingsCount = sourceMap->count();
    NX_DEBUG(this, NX_FMT("Server sent %1 setting(s) to Hanwha plugin to set on camera",
        settingsCount));

    auto errorMap = makePtr<nx::sdk::StringMap>();

    m_settingsProcessor.transferAndHoldSettingsFromServerToDevice(errorMap.get(), sourceMap);

    *outResult = errorMap.releasePtr();
}

//-------------------------------------------------------------------------------------------------

/**
 * Write current plugin settings to the server.
 */
/*virtual*/ void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const /*override*/
{
    const auto response = new nx::sdk::SettingsResponse();

    {
        // This is legal and this function should never have been const anyway.
        auto& self = *const_cast<DeviceAgent*>(this);

        // This is a temporary workaround until the server is fixed to not call this function
        // concurrently.
        std::lock_guard lockGuard(self.m_settingsMutex);

        self.m_settingsProcessor.loadAndHoldSettingsFromDevice();

        m_settingsProcessor.writeSettingsToServer(response);
    }

    *outResult = response;
}

//-------------------------------------------------------------------------------------------------

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/)
{
    const auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto eventMetadataPacket = makePtr<EventMetadataPacket>();

            for (const auto& hanwhaEvent: events)
            {
                if (hanwhaEvent.channel.has_value() && *hanwhaEvent.channel != m_channelNumber)
                    return;

                auto eventMetadata = makePtr<EventMetadata>();
                NX_PRINT
                    << "Got event: caption ["
                    << hanwhaEvent.caption.toStdString() << "], description ["
                    << hanwhaEvent.description.toStdString() << "], "
                    << "channel " << m_channelNumber;

                NX_VERBOSE(this, lm("Got event: %1 %2 on channel %3").args(
                    hanwhaEvent.caption, hanwhaEvent.description, m_channelNumber));

                eventMetadata->setTypeId(hanwhaEvent.typeId.toStdString());
                eventMetadata->setCaption(hanwhaEvent.caption.toStdString());
                eventMetadata->setDescription(hanwhaEvent.description.toStdString());
                eventMetadata->setIsActive(hanwhaEvent.isActive);
                eventMetadata->setConfidence(1.0);
                eventMetadata->addAttribute(makePtr<Attribute>(
                    IAttribute::Type::string,
                    nx::vms::server::analytics::kInputPortIdAttribute,
                    hanwhaEvent.fullEventName.toStdString()));

                eventMetadataPacket->setTimestampUs(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                eventMetadataPacket->setDurationUs(-1);
                eventMetadataPacket->addItem(eventMetadata.get());
            }

            if (NX_ASSERT(m_handler))
                m_handler->handleMetadata(eventMetadataPacket.get());
        };

    if (!NX_ASSERT(m_engine))
        return error(ErrorCode::internalError, "Unable to access the Engine");

    m_monitor = m_engine->monitor(m_sharedId, m_url, m_auth);
    if (!m_monitor)
        return error(ErrorCode::internalError, "Unable to access the monitor");

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return {};
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);
    if (m_engine)
        m_engine->deviceAgentStoppedToUseMonitor(m_sharedId);

    m_monitor = nullptr;
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(QJson::serialized(m_manifest));
}

//-------------------------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------------------------

void DeviceAgent::setManifest(Hanwha::DeviceAgentManifest manifest)
{
    m_manifest = manifest;
    setSupportedEventCategoties();
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::setMonitor(MetadataMonitor* monitor)
{
    m_monitor = monitor;
}

//-------------------------------------------------------------------------------------------------

std::optional<QSet<QString>> DeviceAgent::loadRealSupportedEventTypes() const
{
    return m_settingsProcessor.loadSupportedEventTypes();
}

//-------------------------------------------------------------------------------------------------

std::string DeviceAgent::loadFirmwareVersion() const
{
    return m_settingsProcessor.loadFirmwareVersionFromDevice();
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::loadAndHoldDeviceSettings()
{
    m_settingsProcessor.loadAndHoldSettingsFromDevice();
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha

#include "device_agent.h"
#include "nx/vms_server_plugins/analytics/hanwha/settings_processor.h"

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
#include <nx/sdk/helpers/plugin_diagnostic_event.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/timestamped_object_metadata.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/std/algorithm.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/server/analytics/predefined_attributes.h>

#include <nx/sdk/helpers/string_map.h>

#include <plugins/resource/hanwha/hanwha_request_helper.h>

#include "common.h"
#include "device_response_json_parser.h"

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::network;

//-------------------------------------------------------------------------------------------------

DeviceAgent::DeviceAgent(
    const nx::sdk::IDeviceInfo* deviceInfo,
    Engine* engine,
    const SettingsCapabilities& settingsCapabilities,
    RoiResolution roiResolution,
    bool isNvr,
    const Hanwha::DeviceAgentManifest& deviceAgentManifest)
    :
    m_engine(engine),
    m_settingsCapabilities(settingsCapabilities),
    m_roiResolution(roiResolution),
    m_settings(m_settingsCapabilities, m_roiResolution),
    m_settingsProcessor(
        deviceInfo,
        isNvr,
        m_settings,
        m_roiResolution),
    m_manifest(deviceAgentManifest),
    m_objectMetadataXmlParser(
        url::Builder(deviceInfo->url())
            .setUserName(deviceInfo->login())
            .setPassword(deviceInfo->password())
            .toUrl(),
        m_engine->manifest(),
        m_settings,
        m_engine->objectMetadataAttributeFilters())
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

/*virtual*/ void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler) /*override*/
{
    handler->addRef();
    m_handler.reset(handler);

    setSupportedEventCategoties();
}

//-------------------------------------------------------------------------------------------------

/*virtual*/ void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes) /*override*/
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
/*virtual*/ void DeviceAgent::doPushDataPacket(
    Result<void>* /*outResult*/, IDataPacket* dataPacket) /*override*/
{
    if (!NX_ASSERT(m_handler))
        return;

    if (!NX_ASSERT(dataPacket))
        return;

    const auto incomingPacket = dataPacket->queryInterface<ICustomMetadataPacket>();
    if (!NX_ASSERT(incomingPacket))
        return;

    QByteArray xmlData(incomingPacket->data(), incomingPacket->dataSize());

    auto [outEventPacket, outObjectPacket, outBestShotPackets] =
        m_objectMetadataXmlParser.parse(xmlData, incomingPacket->timestampUs());

    if (outObjectPacket)
        m_handler->handleMetadata(outObjectPacket.get());
    for (auto bestShotPacket: outBestShotPackets)
        m_handler->handleMetadata(bestShotPacket.get());
    if (outEventPacket)
        m_handler->handleMetadata(outEventPacket.get());
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::setSupportedEventCategoties()
{
    if (!m_settingsCapabilities.videoAnalysis2 && m_settingsCapabilities.videoAnalysis)
    {
        NX_ASSERT(m_handler);

        m_handler->handlePluginDiagnosticEvent(new PluginDiagnosticEvent(
            PluginDiagnosticEvent::Level::warning, "IVA settings unavailable",
            "Unfortunately this camera doesn't support modern 'videoanalysis2' API that we use to"
            " configure its Intelligent Video Analytics settings. It only supports older"
            " 'videoanalysis' API, which we currently don't support. This means that IVA settings"
            " are not configurable within the plugin settings. However You can still use the"
            " camera's web interface to configure them."));
    }

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
        m_settingsCapabilities.videoAnalysis2
        && (m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Passing")
            || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Entering")
            || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Exiting")
            || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.AppearDisappear")
            || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Intrusion")
            || m_manifest.supportedEventTypeIds.contains("nx.hanwha.VideoAnalytics.Loitering"));

    m_settings.analyticsCategories[objectDetection] =
        !m_manifest.supportedObjectTypeIds.isEmpty();

    m_settings.analyticsCategories[audioDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioDetection");

    m_settings.analyticsCategories[audioAnalytics] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.Scream")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.Gunshot")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.Explosion")
        || m_manifest.supportedEventTypeIds.contains("nx.hanwha.AudioAnalytics.GlassBreak");

    m_settings.analyticsCategories[faceMaskDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.FaceMaskDetection");

    m_settings.analyticsCategories[temperatureChangeDetection] =
        m_manifest.supportedEventTypeIds.contains("nx.hanwha.TemperatureChangeDetection");
}

//-------------------------------------------------------------------------------------------------

void DeviceAgent::applyWearingMaskBoundingBoxColorSettings(const nx::sdk::IStringMap* settings)
{
    const std::string defaultValue = m_engine->kNoSpecialColorComboBoxItem;
    const char* wearingMaskColor = settings->value("ObjectDetection.FaceMaskDetectedColor");
    if (!wearingMaskColor)
        wearingMaskColor = "";
    if (wearingMaskColor == defaultValue)
        wearingMaskColor = nullptr;

    const char* notWearingMaskColor = settings->value("ObjectDetection.FaceMaskNotDetectedColor");
    if (!notWearingMaskColor)
        notWearingMaskColor = "";
    if (notWearingMaskColor == defaultValue)
        notWearingMaskColor = nullptr;

    const char* notDefinedMaskColor = settings->value("ObjectDetection.FaceMaskNotDefinedColor");
    if (!notDefinedMaskColor)
        notDefinedMaskColor = "";
    if (notDefinedMaskColor == defaultValue)
        notDefinedMaskColor = nullptr;

    m_objectMetadataXmlParser.setWearingMaskBoundingBoxColor(wearingMaskColor);
    m_objectMetadataXmlParser.setNotWearingMaskBoundingBoxColor(notWearingMaskColor);
    m_objectMetadataXmlParser.setNotDefinedMaskBoundingBoxColor(notDefinedMaskColor);
}

/**
 * Read the plugin settings from the server to deviceAgent settings and write them into the
 * agent's device (usually a camera).
 * If a setting can not be written, its name is added to `outResult` error map.
 * if a setting is written successfully, corresponding deviceAgent setting is updated.
*/
/*virtual*/ void DeviceAgent::doSetSettings(
    Result<const ISettingsResponse*>* outResult, const IStringMap* sourceMap) /*override*/
{
    applyWearingMaskBoundingBoxColorSettings(sourceMap);

    if (!m_serverHasSentInitialSettings)
    {
        // First time we should load settings that are not stored on the device.
        m_settingsProcessor.loadAndHoldExclusiveSettingsFromServer(sourceMap);

        m_serverHasSentInitialSettings = true;
        return;
    }

    const int settingsCount = sourceMap->count();
    NX_DEBUG(this, NX_FMT("Server sent %1 setting(s) to Hanwha plugin to set on camera",
        settingsCount));

    auto errorMap = makePtr<nx::sdk::StringMap>();
    auto valueMap = makePtr<nx::sdk::StringMap>();
    m_settingsProcessor.transferAndHoldSettingsFromServerToDevice(
        errorMap.get(), valueMap.get(), sourceMap);

    const int errorCount = errorMap->count();
    if (errorCount)
    {
        NX_DEBUG(this, NX_FMT("While sending setting to Hanwha camera from plugin %1 error(s) occurred",
            errorCount));
        for (int i = 0; i < errorCount; ++i)
        {
            NX_DEBUG(this, NX_FMT("Error %1: key = %2, value = %3", i,
                errorMap->key(i), errorMap->value(i)));
        }
    }
    else
    {
        NX_DEBUG(this, "While sending setting to Hanwha camera from plugin no errors occurred");
    }

    #if 0
        // While plugin manager is under construction its not clear if it is needed.
        settingsResponse->setValues(std::move(valueMap));
    #endif

    auto settingsResponse = makePtr<nx::sdk::SettingsResponse>();
    settingsResponse->setErrors(std::move(errorMap));

    *outResult = settingsResponse.releasePtr();
}

//-------------------------------------------------------------------------------------------------

/**
 * Write current plugin settings to the server.
 */
/*virtual*/ void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const /*override*/
{
    const auto response = new nx::sdk::SettingsResponse();

    // This is temporary. Settings cache should probably be removed entirely.
    {
        // This is a temporary workaround until the server is fixed to not call this function
        // concurrently.
        std::lock_guard lockGuard(m_settingsMutex);

        m_settingsProcessor.transferAndHoldSettingsFromDeviceToServer(response);
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

                if (hanwhaEvent.region)
                    eventMetadata->setKey(std::to_string(*hanwhaEvent.region));

                if (hanwhaEvent.typeId == "nx.hanwha.AlarmInput")
                {
                    eventMetadata->addAttribute(makePtr<Attribute>(
                        IAttribute::Type::string,
                        nx::vms::server::analytics::kInputPortIdAttribute,
                        hanwhaEvent.fullEventName.toStdString()));
                }

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

/*virtual*/ void DeviceAgent::getManifest(Result<const IString*>* outResult) const /*override*/
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

void DeviceAgent::setMonitor(MetadataMonitor* monitor)
{
    m_monitor = monitor;
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

#include "device_agent.h"

#include <chrono>
#include <sstream>
#include <iostream>
#include <charconv>

#include <QtCore/QUrl>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/utils/log/log.h>
#include <nx/vms/server/analytics/predefined_attributes.h>

#include <nx/sdk/helpers/string_map.h>

#include "common.h"

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine):
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    m_engine->deviceAgentIsAboutToBeDestroyed(m_sharedId);
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

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

bool endsWith(std::string_view string, std::string_view ending) //< remove in C++20
{
    return
        ending.size() <= string.size()
        && std::equal(cbegin(ending), cend(ending), cend(string) - ending.size());
}

std::string DeviceAgent::sendCommandToCamera(const std::string& query)
{
    nx::utils::Url command(m_url);
    constexpr const char* kEventPath = "/stw-cgi/eventsources.cgi";
    command.setPath(kEventPath);
    command.setQuery(QString::fromStdString(query));

    const bool isSent = m_settingsHttpClient.doGet(command);
    if (!isSent)
        return "Failed to send command to camera";

    auto* response = m_settingsHttpClient.response();
    const bool isApproved = (response->statusLine.statusCode == 200);
    if (!isApproved)
        return response->statusLine.toString().toStdString();

    return {};
}

static std::string dequote(const std::string& s)
{
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

/*static*/ void DeviceAgent::replanishErrorMap(nx::sdk::Ptr<nx::sdk::StringMap>& errorMap,
    AnalyticsParamSpan params, const char* reason)
{
    for (const auto param: params)
        errorMap->setItem(param.plugin, reason);
}

template<class S, class F>
void retr2(nx::sdk::Ptr<nx::sdk::StringMap>& errorMap,
    const nx::sdk::IStringMap* settings,
    S& previousState,
    F f,
    FrameSize frameSize,
    int objectIndex = 0)
{
    S settingGroup;
    if (!settingGroup.loadFromServer(settings, objectIndex))
        settingGroup.replanishErrorMap(errorMap, "read failed");
    else
    {
        if (settingGroup.differesEnoughFrom(previousState))
        {
            const std::string query = buildCameraRequestQuery(settingGroup, frameSize);
                const std::string error = f(query);
                if (!error.empty())
                    settingGroup.replanishErrorMap(errorMap, error);
                else
                    previousState = settingGroup;
        }
    }

}

constexpr const char* failedToReceiveError = "Failed to receive a value from server";

void DeviceAgent::doSetSettings(
    Result<const IStringMap*>* outResult, const IStringMap* settings)
{

    int c = settings->count();
    //std::shared_ptr<vms::server::plugins::HanwhaSharedResourceContext> m_engine->sharedContext(m_sharedId);

    auto errorMap = makePtr<nx::sdk::StringMap>();

    const auto sender = [this](const std::string& s) {return this->sendCommandToCamera(s); };

    retr2(errorMap, settings, m_settings.shockDetection, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.motion, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.mdObjectSize, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.ivaObjectSize, sender, m_frameSize);

    for (int i = 0; i < 8; ++i)
        retr2(errorMap, settings, m_settings.mdIncludeArea[i], sender, m_frameSize, i + 1);
    for (int i = 0; i < 8; ++i)
        retr2(errorMap, settings, m_settings.mdExcludeArea[i], sender, m_frameSize, i + 1);

    retr2(errorMap, settings, m_settings.tamperingDetection, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.defocusDetection, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.odObjects, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.odBestShot, sender, m_frameSize);

    for (int i = 0; i < 8; ++i)
        retr2(errorMap, settings, m_settings.odExcludeArea[i], sender, m_frameSize, i + 1);

    for (int i = 0; i < 8; ++i)
        retr2(errorMap, settings, m_settings.ivaLine[i], sender, m_frameSize, i + 1);

    for (int i = 0; i < 8; ++i)
        retr2(errorMap, settings, m_settings.ivaIncludeArea[i], sender, m_frameSize, i + 1);

    for (int i = 0; i < 8; ++i)
        retr2(errorMap, settings, m_settings.ivaExcludeArea[i], sender, m_frameSize, i + 1);

    retr2(errorMap, settings, m_settings.audioDetection, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.soundClassification, sender, m_frameSize);

    *outResult = errorMap.releasePtr();
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* /*outResult*/) const
{
}

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/)
{
    const auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto eventMetadataPacket = new EventMetadataPacket();

            for (const auto& hanwhaEvent: events)
            {
                if (hanwhaEvent.channel.is_initialized() && hanwhaEvent.channel != m_channelNumber)
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
                eventMetadata->setDescription(hanwhaEvent.caption.toStdString());
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
                m_handler->handleMetadata(eventMetadataPacket);

            eventMetadataPacket->releaseRef();
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

void DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);
    if (m_engine)
        m_engine->deviceAgentStoppedToUseMonitor(m_sharedId);

    m_monitor = nullptr;
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    if (m_deviceAgentManifest.isEmpty())
        *outResult = error(ErrorCode::internalError, "DeviceAgent manifest is empty");
    else
        *outResult = new nx::sdk::String(m_deviceAgentManifest);
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

    m_settingsHttpClient.setUserName(m_auth.user());
    m_settingsHttpClient.setUserPassword(m_auth.password());
    m_settingsHttpClient.addAdditionalHeader("Accept", "application/json");
}

void DeviceAgent::setDeviceAgentManifest(const QByteArray& manifest)
{
    m_deviceAgentManifest = manifest;
}

void DeviceAgent::setEngineManifest(const Hanwha::EngineManifest& manifest)
{
    m_engineManifest = manifest;
}

void DeviceAgent::setMonitor(MetadataMonitor* monitor)
{
    m_monitor = monitor;
}

std::string DeviceAgent::loadSunapiObject(const char* eventName)
{
    std::string result;
    nx::utils::Url command(m_url);
    constexpr const char* kPath = "/stw-cgi/eventsources.cgi";
    constexpr const char* kQueryPattern = "msubmenu=%1&action=view&Channel=%2";
    command.setPath(kPath);
    command.setQuery(QString::fromStdString(kQueryPattern).arg(eventName).arg(m_channelNumber));

    const bool isSent = m_settingsHttpClient.doGet(command);
    if (!isSent)
        return result;

    auto* response = m_settingsHttpClient.response();
    const bool isApproved = (response->statusLine.statusCode == 200);
    const std::string sl = response->statusLine.toString().toStdString();

    auto messageBodyOptional = m_settingsHttpClient.fetchEntireMessageBody();
    if (messageBodyOptional.has_value())
        result = messageBodyOptional->toStdString();
    else
        ;//NX_DEBUG(logTag, "makeActiRequest: Error getting response body.");
    return result; // nx::network::http::StatusCode::internalServerError;

//    m_settings.shockDetection.loadFromSunapi(sss.c_str());

}

void DeviceAgent::readCameraSettings()
{
    std::string sunapiString = loadSunapiObject("shockdetection");
    m_settings.shockDetection.loadFromSunapi(sunapiString, m_channelNumber);

    sunapiString = loadSunapiObject("tamperingdetection");
    m_settings.tamperingDetection.loadFromSunapi(sunapiString, m_channelNumber);

    sunapiString = loadSunapiObject("videoanalysis2");
    m_settings.motion.loadFromSunapi(sunapiString, m_channelNumber);
    m_settings.mdObjectSize.loadFromSunapi(sunapiString, m_channelNumber);

    for (int i = 0; i < 8; ++i)
        m_settings.mdIncludeArea[i].loadFromSunapi(sunapiString, m_channelNumber, i + 1);

    for (int i = 0; i < 8; ++i)
        m_settings.mdExcludeArea[i].loadFromSunapi(sunapiString, m_channelNumber, i + 1);

    m_settings.ivaObjectSize.loadFromSunapi(sunapiString, m_channelNumber);

    for (int i = 0; i < 8; ++i)
        m_settings.ivaLine[i].loadFromSunapi(sunapiString, m_channelNumber, i + 1);

    for (int i = 0; i < 8; ++i)
        m_settings.ivaIncludeArea[i].loadFromSunapi(sunapiString, m_channelNumber, i + 1);

    for (int i = 0; i < 8; ++i)
        m_settings.ivaExcludeArea[i].loadFromSunapi(sunapiString, m_channelNumber, i + 1);

    sunapiString = loadSunapiObject("defocusdetection");
    m_settings.defocusDetection.loadFromSunapi(sunapiString, m_channelNumber);

    sunapiString = loadSunapiObject("objectdetection");
    m_settings.odObjects.loadFromSunapi(sunapiString, m_channelNumber);
    m_settings.odBestShot.loadFromSunapi(sunapiString, m_channelNumber);
    for (int i = 0; i < 8; ++i)
        m_settings.odExcludeArea[i].loadFromSunapi(sunapiString, m_channelNumber, i + 1);

    sunapiString = loadSunapiObject("audiodetection");
    m_settings.audioDetection.loadFromSunapi(sunapiString, m_channelNumber);

    sunapiString = loadSunapiObject("audioanalysis");
    m_settings.soundClassification.loadFromSunapi(sunapiString, m_channelNumber);
}

} // namespace nx::vms_server_plugins::analytics::hanwha

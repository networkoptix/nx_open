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

// read the setting from the server to plugin
// returns empty vector, if not all values can be read
std::vector<std::string> DeviceAgent::ReadSettingsFromServer(
    const IStringMap* settings,
    AnalyticsParamSpan analyticsParamSpan,
    int objectIndex)
{
    std::vector<std::string> result;
    result.reserve(std::size(analyticsParamSpan));
    for (auto param: analyticsParamSpan)
    {
        const char* const value =
            upperIfBoolean(settings->value(specify(param.plugin, objectIndex).c_str()));
        if (!value)
        {
            result.clear();
            return result;
        }

        if (endsWith(param.plugin, ".Points"))
        {
            std::optional<std::vector<PluginPoint>> points = parsePluginPoints(value);
            if (!points)
            {
                result.clear();
                return result;
            }

            std::string sunapiPoints = pluginPointsToSunapiString(*points, m_frameSize);
            result.push_back(sunapiPoints);
        }
        else
        {
            result.emplace_back(value);
        }
    }
    return result;
}

std::string DeviceAgent::sendCommand(const std::string& query)
{
    nx::utils::Url command(m_url);
    constexpr const char* kEventPath = "/stw-cgi/eventsources.cgi";
    command.setPath(kEventPath);
    command.setQuery(QString::fromStdString(query) + QString("&Channel=%1").arg(m_channelNumber));

    const bool isSent = m_settingsHttpClient.doGet(command);
    if (!isSent)
        return "Failed to send command to camera";

    auto* response = m_settingsHttpClient.response();
    const bool isApproved = (response->statusLine.statusCode == 200);
    if (!isApproved)
        return response->statusLine.toString().toStdString();

    return {};
}

std::string DeviceAgent::sendCommand2(const std::string& query)
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

std::string DeviceAgent::WriteSettingsToCamera(
    const std::vector<std::string>& values,
    AnalyticsParamSpan analyticsParamSpan,
    const char* commandPreambule,
    int objectIndex)
{
    NX_ASSERT(std::size(values) == std::size(analyticsParamSpan));

    std::stringstream query;
    query << commandPreambule;

    auto paramIt = analyticsParamSpan.begin();
    auto valueIt = values.cbegin();

    while (valueIt != values.end())
    {
        query << specify(paramIt->sunapi, objectIndex) << dequote(*valueIt);
        ++paramIt;
        ++valueIt;
    }

    return sendCommand(query.str());
}

// Used for deleting commands.
std::string DeviceAgent::WriteSettingsToCamera(
    const char* query,
    int objectIndex)
{
    const std::string specifiedQuery = specify(query, objectIndex);
    return sendCommand(specifiedQuery);
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

    const auto sender = [this](const std::string& s) {return this->sendCommand2(s); };

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
    retr2(errorMap, settings, m_settings.objectDetectionObjects, sender, m_frameSize);
    retr2(errorMap, settings, m_settings.objectDetectionBestShot, sender, m_frameSize);

    for (int i = 0; i < 8; ++i)
        retr2(errorMap, settings, m_settings.objectDetectionExcludeArea[i], sender, m_frameSize, i + 1);

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

} // namespace nx::vms_server_plugins::analytics::hanwha

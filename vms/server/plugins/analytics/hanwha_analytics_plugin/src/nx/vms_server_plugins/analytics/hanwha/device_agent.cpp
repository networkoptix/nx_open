#include "device_agent.h"

#include <chrono>
#include <sstream>

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

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

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

const char* upperBoolean(const char* value)
{
    if (!value)
        return nullptr;
    if (strcmp(value, "false") == 0)
        return "False";
    if (strcmp(value, "true") == 0)
        return "True";
    return value;
}

void DeviceAgent::updateCameraEventSetting(
    const IStringMap* settings,
    const char* commandPreambule,
    AnalyticsParamSpan analyticsParamSpan,
   Ptr<StringMap>& errorMap)
{

    std::stringstream query;
    query << commandPreambule;

    bool areParametersRead = true; //< optimistic prediction

    for (auto param: analyticsParamSpan)
    {
        if (const auto value = upperBoolean(settings->value(param.plugin)); value)
        {
            query << param.sunapi << value;
        }
        else //< very unlikely
        {
            errorMap->setItem(param.plugin, "Failed to receive a value from server");
            areParametersRead = false;
        }
    }
    if (!areParametersRead)
        return;

    nx::utils::Url command(m_url);
    constexpr const char* kEventPath = "/stw-cgi/eventsources.cgi";
    command.setPath(kEventPath);
    command.setQuery(QString::fromStdString(query.str()));

    const bool isSent = m_settingsHttpClient.doGet(command);
    bool isApproved = false;
    std::string errorMessage;
    if (!isSent)
    {
        errorMessage = "Failed to send command to camera.";
    }
    else
    {
        auto* response = m_settingsHttpClient.response();
        isApproved = (response->statusLine.statusCode == 200);
        if (!isApproved)
            errorMessage = response->statusLine.toString().toStdString();
    }
    if (!isSent || !isApproved)
    {
        for (const auto& param: analyticsParamSpan)
            errorMap->setItem(param.plugin, errorMessage.c_str());
    }

}

void DeviceAgent::doSetSettings(
    Result<const IStringMap*>* outResult, const IStringMap* settings)
{
    auto errorMap = makePtr<nx::sdk::StringMap>();
    std::string errorMessage;

    {
        // 1. ShockDetection. SUNAPI 2.5.4 (2018-08-07) 2.23
        constexpr const char* kShockPreambule = "msubmenu=shockdetection&action=set";
        constexpr AnalyticsParam kShockParams[] = {
            { "ShockDetection.Enable", "&Enable=" },
            { "ShockDetection.ThresholdLevel", "&ThresholdLevel=" },
            { "ShockDetection.Sensitivity", "&Sensitivity=" }
        };
        updateCameraEventSetting(settings, kShockPreambule, kShockParams, errorMap);
    }
    {
        // 2. MotionDetection
    }
    {
        // 3. TamperingDetection 2.6
        constexpr const char* kTamperingPreambule = "msubmenu=tamperingdetection&action=set";
        constexpr AnalyticsParam kTamperingParams[] = {
            { "TamperingDetection.Enable", "&Enable=" },
            { "TamperingDetection.ThresholdLevel", "&ThresholdLevel=" },
            { "TamperingDetection.SensitivityLevel", "&SensitivityLevel=" },
            { "TamperingDetection.MinimumDuration", "&Duration=" },
            { "TamperingDetection.ExceptDarkImages", "&DarknessDetection=" }
        };
        updateCameraEventSetting(settings, kTamperingPreambule, kTamperingParams, errorMap);
    }

    {
        // 4. DefocusDetection SUNAPI 2.5.4 (2018-08-07) 2.14
        constexpr const char* kDefocusPreambule = "msubmenu=defocusdetection&action=set";
        constexpr AnalyticsParam kDefocuseParams[] = {
            { "DefocusDetection.Enable", "&Enable=" },
            { "DefocusDetection.ThresholdLevel", "&ThresholdLevel=" },
            { "DefocusDetection.Sensitivity", "&Sensitivity=" },
            { "DefocusDetection.MinimumDuration", "&Duration=" }
        };
        updateCameraEventSetting(settings, kDefocusPreambule, kDefocuseParams, errorMap);
    }

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

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

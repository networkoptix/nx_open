// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <cstring>

#include <nx/kit/json.h>
#include <nx/kit/utils.h>
#include <nx/sdk/helpers/active_setting_changed_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../utils.h"

#include "stub_analytics_plugin_error_reporting_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace error_reporting {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId())
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    if (ini().returnIncorrectDeviceAgentManifest)
        return "incorrectDeviceAgentManifest";

    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "capabilities": "",
    "supportedTypes":
    [
    ],
    "typeLibrary":
    {
    }
})json";
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* /*videoFrame*/)
{
    ++m_frameCount;

    if (ini().errorOnPushCompressedFrame && (m_frameCount % 100 == 0))
        return false;

    return true;
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* /*metadataPackets*/)
{
    return true;
}

void DeviceAgent::doSetSettings(
    nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
    const nx::sdk::IStringMap* /*settings*/)
{
    if (std::strcmp(ini().returnErrorFromDeviceAgentSetSettings,
        Ini::kErrorInsteadOfSettingsResponse) == 0)
    {
        *outResult = error(ErrorCode::internalError,
            "DeviceAgent::setSettings() intentionally failed.");
    }
    else if (std::strcmp(ini().returnErrorFromDeviceAgentSetSettings,
        Ini::kSettingsResponseWithError) == 0)
    {
       auto settingsResponse = makePtr<SettingsResponse>();

       settingsResponse->setError("DeviceAgent SetSettings Error Key",
            "DeviceAgent SetSettings Error Value");

       *outResult = settingsResponse.releasePtr();
    }
}

void DeviceAgent::doGetSettingsOnActiveSettingChange(
    Result<const IActiveSettingChangedResponse*>* outResult,
    const IActiveSettingChangedAction* /*activeSettingChangedAction*/)
{
    if (std::strcmp(ini().returnErrorFromDeviceAgentActiveSettingChange,
        Ini::kErrorInsteadOfSettingsResponse) == 0)
    {
        *outResult = error(ErrorCode::internalError, "DeviceAgent::getSettingsOnActiveSettingChange() intentionally failed.");
    }
    else if (std::strcmp(ini().returnErrorFromDeviceAgentActiveSettingChange,
        Ini::kSettingsResponseWithError) == 0)
    {
        const auto settingsResponse = makePtr<SettingsResponse>();

        settingsResponse->setError("DeviceAgent GetSettingsOnActiveSettingChange Error Key",
            "DeviceAgent GetSettingsOnActiveSettingChange Error Value");

        auto activeSettingChangedResponse = makePtr<ActiveSettingChangedResponse>();
        activeSettingChangedResponse->setSettingsResponse(settingsResponse);

        *outResult = activeSettingChangedResponse.releasePtr();
    }
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    if (std::strcmp(ini().returnErrorFromDeviceAgentOnGetPluginSideSettings,
        Ini::kErrorInsteadOfSettingsResponse) == 0)
    {
        *outResult = error(ErrorCode::internalError,
            "Unable to get Plugin Side Settings in DeviceAgent.");
    }
    else if (std::strcmp(ini().returnErrorFromDeviceAgentOnGetPluginSideSettings,
        Ini::kSettingsResponseWithError) == 0)
    {
        auto settingsResponse = makePtr<SettingsResponse>();

        settingsResponse->setError("DeviceAgent getPluginSideSettings Error Key",
            "DeviceAgent getPluginSideSettings Error Value");

        *outResult = settingsResponse.releasePtr();
    }
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    return nullptr;
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* /*neededMetadataTypes*/)
{
}

} // namespace error_reporting
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

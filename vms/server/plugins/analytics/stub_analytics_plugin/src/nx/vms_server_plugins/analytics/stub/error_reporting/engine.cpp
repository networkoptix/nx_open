// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/sdk/helpers/active_setting_changed_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../utils.h"
#include "device_agent.h"
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

Engine::Engine(Integration* integration):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, integration->instanceId()),
    m_integration(integration)
{
}

Engine::~Engine()
{
}


std::string Engine::manifestString() const
{
    if (ini().returnIncorrectEngineManifest)
        return "incorrectEngineManifest";

    return /*suppress newline*/ 1 + (const char*)R"json(
{
    "typeLibrary":
    {
    },
    "capabilities": "",
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "CheckBox",
                "name": "Test Device Agent's setSettings",
                "caption": "Test Device Agent's setSettings()",
                "defaultValue": false
            },
            {
                "type": "CheckBox",
                "name": "Test Device Agent's Active Settings",
                "caption": "Test Device Agent's Active Settings",
                "defaultValue": false,
                "isActive": true
            },
            {
                "type": "TextField",
                "name": "DeviceAgent SetSettings Error Key",
                "caption": "",
                "defaultValue": "",
                "visible": false
            },
            {
                "type": "TextField",
                "name": "DeviceAgent GetSettingsOnActiveSettingChange Error Key",
                "caption": "",
                "defaultValue": "",
                "visible": false
            },
            {
                "type": "TextField",
                "name": "DeviceAgent getIntegrationSideSettings Error Key",
                "caption": "",
                "defaultValue": "",
                "visible": false
            }
        ]
    }
}
)json";
}

void Engine::doSetSettings(
    nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
    const nx::sdk::IStringMap* /*settings*/)
{
    if (std::strcmp(ini().returnErrorFromEngineSetSettings,
        Ini::kErrorInsteadOfSettingsResponse) == 0)
    {
        *outResult = error(ErrorCode::internalError, "Unable to set Engine settings.");
    }
    else if (std::strcmp(ini().returnErrorFromEngineSetSettings,
        Ini::kSettingsResponseWithError) == 0)
    {
        auto settingsResponse = makePtr<SettingsResponse>();

        settingsResponse->setError("Engine SetSettings Error Key",
            "Engine SetSettings Error Value");

        *outResult = settingsResponse.releasePtr();
    }
}

Result<const ISettingsResponse*> Engine::settingsReceived()
{
    return nullptr;
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    if (ini().returnNullOnDeviceAgentCreation)
    {
        *outResult = nullptr;
        return;
    }
    else if (ini().returnErrorOnDeviceAgentCreation)
    {
        *outResult = error(ErrorCode::internalError, "Unable to create DeviceAgent instance.");
        return;
    }

    *outResult = new DeviceAgent(this, deviceInfo);
}

void Engine::doGetSettingsOnActiveSettingChange(
    Result<const IActiveSettingChangedResponse*>* outResult,
    const IActiveSettingChangedAction* /*activeSettingChangedAction*/)
{
    if (std::strcmp(ini().returnErrorFromEngineActiveSettingChange,
        Ini::kErrorInsteadOfSettingsResponse) == 0)
    {
        *outResult = error(ErrorCode::internalError, "Unable to set Active Settings in Engine.");
    }
    else if (std::strcmp(ini().returnErrorFromEngineActiveSettingChange,
        Ini::kSettingsResponseWithError) == 0)
    {
        const auto settingsResponse = makePtr<SettingsResponse>();

        settingsResponse->setError("Engine GetSettingsOnActiveSettingChange Error Key",
            "Engine GetSettingsOnActiveSettingChange Error Value");

        auto activeSettingChangedResponse = makePtr<ActiveSettingChangedResponse>();
        activeSettingChangedResponse->setSettingsResponse(settingsResponse);

        *outResult = activeSettingChangedResponse.releasePtr();
    }
}

void Engine::getIntegrationSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const
{
    if (std::strcmp(ini().returnErrorFromEngineOnGetIntegrationSideSettings,
        Ini::kErrorInsteadOfSettingsResponse) == 0)
    {
        *outResult = error(ErrorCode::internalError,
            "Unable to get Plugin-side settings in Engine.");
    }
    else if (std::strcmp(ini().returnErrorFromEngineOnGetIntegrationSideSettings,
        Ini::kSettingsResponseWithError) == 0)
    {
        auto settingsResponse = makePtr<SettingsResponse>();

        settingsResponse->setError("Engine getIntegrationSideSettings Error Key",
            "Engine getIntegrationSideSettings Error Value");

        *outResult = settingsResponse.releasePtr();
    }
}

} // namespace error_reporting
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

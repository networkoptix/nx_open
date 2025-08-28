// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <iostream>

#include <nx/kit/debug.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/settings_response.h>

#include "device_agent.h"
#include "mqtt_proxy_ini.h"

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

using nx::kit::Json;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(PythonAppHost& pythonAppHost):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT), m_pythonAppHost(pythonAppHost)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

std::string Engine::manifestString() const
{
    const std::optional<std::string> objectActionsModel = m_pythonAppHost.callPythonFunctionWithRv(
        "object_actions_model", {});
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "objectActions": )json" + (objectActionsModel ? objectActionsModel.value() : "[]") + R"json(
}
)json";
}

Result<const ISettingsResponse*> Engine::settingsReceived()
{
    const std::map<std::string, std::string> values = currentSettings();

    auto settingsResponse = new SettingsResponse();
    applySettingsToPythonApp(values);
    settingsResponse->setValues(makePtr<StringMap>(values));

    return settingsResponse;
}

void Engine::applySettingsToPythonApp(const std::map<std::string, std::string>& values)
{
    m_pythonAppHost.callPythonFunction("apply_settings", values);
}

Result<IAction::Result> Engine::executeAction(
    const std::string& actionId,
    [[maybe_unused]] Uuid trackId,
    [[maybe_unused]] Uuid deviceId,
    [[maybe_unused]] int64_t timestampUs,
    [[maybe_unused]] Ptr<IObjectTrackInfo> objectTrackInfo,
    const std::map<std::string, std::string>& params)
{
    std::map<std::string, std::string> pythonParams = params;
    pythonParams["_action_id"] = actionId;

    const std::optional<std::string> actionResult = m_pythonAppHost.callPythonFunctionWithRv(
        "execute_object_action", pythonParams);

    IAction::Result result;

    if (actionResult)
        result.messageToUser = makePtr<String>(actionResult.value());
    else
        result.messageToUser = makePtr<String>("Action failed: error in the Python part.");

    return result;
}

} // namespace nx::vms_server_plugins::analytics::mqtt_proxy

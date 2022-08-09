// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <algorithm>

#include "actions.h"
#include "active_settings_rules.h"
#include "device_agent.h"
#include "settings_model.h"
#include "stub_analytics_plugin_settings_ini.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/sdk/helpers/active_setting_changed_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/vms_server_plugins/analytics/stub/utils.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
    m_plugin(plugin)
{
    for (const auto& entry: kActiveSettingsRules)
    {
        const ActiveSettingsBuilder::ActiveSettingKey key = entry.first;
        m_activeSettingsBuilder.addRule(
            key.activeSettingId,
            key.activeSettingValue,
            /*activeSettingHandler*/ entry.second);
    }

    for (const auto& entry: kDefaultActiveSettingsRules)
    {
        m_activeSettingsBuilder.addDefaultRule(
            /*activeSettingId*/ entry.first,
            /*activeSettingHandler*/ entry.second);
    }
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

static std::string buildCapabilities()
{
    std::string capabilities;

    if (ini().deviceDependent)
        capabilities += "|deviceDependent";

    // Delete first '|', if any.
    if (!capabilities.empty() && capabilities.at(0) == '|')
        capabilities.erase(0, 1);

    return capabilities;
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": ")json" + buildCapabilities() + R"json(",
    "deviceAgentSettingsModel":
)json"
        + kRegularSettingsModelPart1
        + kEnglishCitiesSettingsModelPart
        + kRegularSettingsModelPart2
        + R"json(
}
)json";

    return result;
}

Result<const ISettingsResponse*> Engine::settingsReceived()
{
    auto settingsResponse = new SettingsResponse();
    settingsResponse->setValue(kEnginePluginSideSetting, kEnginePluginSideSettingValue);

    return settingsResponse;
}

void Engine::getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const
{
    auto settingsResponse = new SettingsResponse();
    settingsResponse->setValue(kEnginePluginSideSetting, kEnginePluginSideSettingValue);

    *outResult = settingsResponse;
}

void Engine::doGetSettingsOnActiveSettingChange(
    Result<const IActiveSettingChangedResponse*>* outResult,
    const IActiveSettingChangedAction* activeSettingChangeAction)
{
    using namespace nx::kit;

    std::string parseError;
    Json::object model = Json::parse(
        activeSettingChangeAction->settingsModel(), parseError).object_items();
    Json::array items = model[kItems].array_items();

    auto activeSettingsGroupBoxIt = std::find_if(items.begin(), items.end(),
        [](Json& item)
        {
            return item[kCaption].string_value() == kActiveSettingsGroupBoxCaption;
        });

    if (activeSettingsGroupBoxIt == items.cend())
    {
        *outResult = error(ErrorCode::internalError, "Unable to find the active settings section");
        return;
    }

    const std::string settingId(activeSettingChangeAction->activeSettingId());
    Json activeSettingsItems = (*activeSettingsGroupBoxIt)[kItems];
    std::map<std::string, std::string> values = toStdMap(shareToPtr(
        activeSettingChangeAction->settingsValues()));

    m_activeSettingsBuilder.updateSettings(settingId, &activeSettingsItems, &values);

    Json::array updatedActiveSettingsItems = activeSettingsItems.array_items();
    Json::object updatedActiveGroupBox = activeSettingsGroupBoxIt->object_items();
    updatedActiveGroupBox[kItems] = updatedActiveSettingsItems;
    *activeSettingsGroupBoxIt = updatedActiveGroupBox;
    model[kItems] = items;

    const auto settingsResponse = makePtr<SettingsResponse>();
    settingsResponse->setValues(makePtr<StringMap>(values));
    settingsResponse->setModel(makePtr<String>(Json(model).dump()));

    const nx::sdk::Ptr<nx::sdk::ActionResponse> actionResponse =
        generateActionResponse(settingId, activeSettingChangeAction->params());

    auto response = makePtr<ActiveSettingChangedResponse>();
    response->setSettingsResponse(settingsResponse);
    response->setActionResponse(actionResponse);

    *outResult = response.releasePtr();
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

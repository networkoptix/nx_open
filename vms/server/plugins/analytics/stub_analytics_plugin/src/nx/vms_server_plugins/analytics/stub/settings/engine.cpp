// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <algorithm>

#include "actions.h"
#include "active_settings_rules.h"
#include "device_agent.h"
#include "settings_model.h"
#include "stub_analytics_plugin_settings_ini.h"

#include <nx/sdk/helpers/active_setting_changed_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/vms_server_plugins/analytics/stub/utils.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::kit;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
    m_plugin(plugin)
{
    for (const auto& entry: kActiveSettingsRules)
    {
        const ActiveSettingsBuilder::ActiveSettingKey key = entry.first;
        m_activeSettingsBuilder.addRule(
            key.activeSettingName,
            key.activeSettingValue,
            /*activeSettingHandler*/ entry.second);
    }

    for (const auto& entry: kDefaultActiveSettingsRules)
    {
        m_activeSettingsBuilder.addDefaultRule(
            /*activeSettingName*/ entry.first,
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

bool Engine::processActiveSettings(
    Json::object* model,
    std::map<std::string, std::string>* values,
    const std::vector<std::string>& settingIdsToUpdate) const
{
    Json::array items = (*model)[kItems].array_items();

    auto activeSettingsGroupBoxIt = std::find_if(items.begin(), items.end(),
        [](Json& item)
        {
            return item[kCaption].string_value() == kActiveSettingsGroupBoxCaption;
        });

    if (activeSettingsGroupBoxIt == items.cend())
        return false;

    Json activeSettingsItems = (*activeSettingsGroupBoxIt)[kItems];

    std::vector<std::string> activeSettingNames = settingIdsToUpdate;
    if (activeSettingNames.empty())
    {
        for (const auto item : activeSettingsItems.array_items())
        {
            if (item["type"].string_value() == "Button")
                continue;

            std::string name = item[kName].string_value();
            activeSettingNames.push_back(name);
        }
    }

    for (const auto& settingId: activeSettingNames)
        m_activeSettingsBuilder.updateSettings(settingId, &activeSettingsItems, values);

    Json::array updatedActiveSettingsItems = activeSettingsItems.array_items();
    Json::object updatedActiveGroupBox = activeSettingsGroupBoxIt->object_items();
    updatedActiveGroupBox[kItems] = updatedActiveSettingsItems;
    *activeSettingsGroupBoxIt = updatedActiveGroupBox;
    (*model)[kItems] = items;

    return true;
}

Result<const ISettingsResponse*> Engine::settingsReceived()
{
    std::string parseError;
    Json::object model = Json::parse(kEngineSettingsModel, parseError).object_items();

    std::map<std::string, std::string> values = currentSettings();
    values[kEnginePluginSideSetting] = kEnginePluginSideSettingValue;

    if (!processActiveSettings(&model, &values))
        return error(ErrorCode::internalError, "Unable to process the active settings section");

    auto settingsResponse = new SettingsResponse();
    settingsResponse->setModel(makePtr<String>(Json(model).dump()));
    settingsResponse->setValues(makePtr<StringMap>(values));

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
    const IActiveSettingChangedAction* activeSettingChangedAction)
{
    std::string parseError;
    Json::object model = Json::parse(
        activeSettingChangedAction->settingsModel(), parseError).object_items();

    const std::string settingId(activeSettingChangedAction->activeSettingName());

    std::map<std::string, std::string> values = toStdMap(shareToPtr(
        activeSettingChangedAction->settingsValues()));

    if (!processActiveSettings(&model, &values, {settingId}))
    {
        *outResult =
            error(ErrorCode::internalError, "Unable to process the active settings section");

        return;
    }

    const auto settingsResponse = makePtr<SettingsResponse>();
    settingsResponse->setValues(makePtr<StringMap>(values));
    settingsResponse->setModel(makePtr<String>(Json(model).dump()));

    const nx::sdk::Ptr<nx::sdk::ActionResponse> actionResponse =
        generateActionResponse(settingId, activeSettingChangedAction->params(), values);

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

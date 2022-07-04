// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <algorithm>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/json.h>
#include <nx/kit/utils.h>

#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/active_setting_changed_response.h>
#include <nx/vms_server_plugins/analytics/stub/utils.h>

#include "active_settings_rules.h"
#include "settings_model.h"
#include "stub_analytics_plugin_settings_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId()),
    m_engine(engine)
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

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": "disableStreamSelection"
}
)json";
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    const auto settingsResponse = new sdk::SettingsResponse();

    const std::string settingsModel = settingValue(kSettingsModelSettings);

    if (ini().sendSettingsModelWithValues)
    {
        if (settingsModel == kAlternativeSettingsModelOption)
        {
            settingsResponse->setModel(kAlternativeSettingsModel);
        }
        else if (settingsModel == kRegularSettingsModelOption)
        {
            const std::string languagePart = (settingValue(kCitySelector) == kGermanOption)
                ? kGermanCitiesSettingsModelPart
                : kEnglishCitiesSettingsModelPart;

            settingsResponse->setModel(kRegularSettingsModelPart1
                + languagePart
                + kRegularSettingsModelPart2);
        }
    }

    // The manifest depends on some of the above parsed settings, so sending the new manifest.
    pushManifest(manifestString());

    return settingsResponse;
}

void DeviceAgent::dumpStringMap(
    const char* prefix, const char* appendix, const IStringMap* stringMap) const
{
    if (!stringMap)
    {
        NX_PRINT << prefix << "    null" << appendix;
        return;
    }

    NX_PRINT << prefix << "{";
    for (int i = 0; i < stringMap->count(); ++i)
    {
        NX_PRINT << prefix << "    " << nx::kit::utils::toString(stringMap->key(i)) << ": "
            << nx::kit::utils::toString(stringMap->value(i))
            << ((i == stringMap->count() - 1) ? "" : ",");
    }
    NX_PRINT << prefix << "}" << appendix;
}

void DeviceAgent::dumpActiveSettingChangedAction(
    const IActiveSettingChangedAction* activeSettingChangeAction) const
{
    NX_PRINT << "IActiveSettingChangedAction:";
    NX_PRINT << "{";
    NX_PRINT << "    \"activeSettingId\": "
        << nx::kit::utils::toString(activeSettingChangeAction->activeSettingId()) << ",";
    NX_PRINT << "    \"settingsModel\": "
        << nx::kit::utils::toString(activeSettingChangeAction->settingsModel()) << ",";
    NX_PRINT << "    \"settingsValues\":";
    dumpStringMap(/*prefix*/ "    ", /*appendix*/ ",",
        activeSettingChangeAction->settingsValues().get());
    NX_PRINT << "    \"params\":";
    dumpStringMap(/*prefix*/ "    ", /*appendix*/ "",
        activeSettingChangeAction->params().get());
    NX_PRINT << "}";
}

void DeviceAgent::doGetSettingsOnActiveSettingChange(
    Result<const IActiveSettingChangedResponse*>* outResult,
    const IActiveSettingChangedAction* activeSettingChangeAction)
{
    if (NX_DEBUG_ENABLE_OUTPUT)
        dumpActiveSettingChangedAction(activeSettingChangeAction);

    using namespace nx::kit;

    std::string parseError;
    Json::object model = Json::parse(
        activeSettingChangeAction->settingsModel(), parseError).object_items();
    Json::array sections = model[kSections].array_items();

    auto activeSettingsSectionIt = std::find_if(sections.begin(), sections.end(),
        [](Json& section)
        {
            return section[kCaption].string_value() == kActiveSettingsSectionCaption;
        });

    if (activeSettingsSectionIt == sections.cend())
    {
        *outResult = error(ErrorCode::internalError, "Unable to find the active settings section");
        return;
    }

    const std::string settingId(activeSettingChangeAction->activeSettingId());
    Json activeSettingsItems = (*activeSettingsSectionIt)[kItems];
    std::map<std::string, std::string> values = toStdMap(shareToPtr(
        activeSettingChangeAction->settingsValues()));

    m_activeSettingsBuilder.updateSettings(settingId, &activeSettingsItems, &values);

    Json::array updatedActiveSettingsItems = activeSettingsItems.array_items();
    Json::object updatedActiveSection = activeSettingsSectionIt->object_items();
    updatedActiveSection[kItems] = updatedActiveSettingsItems;
    *activeSettingsSectionIt = updatedActiveSection;
    model[kSections] = sections;

    const auto settingsResponse = makePtr<SettingsResponse>();
    settingsResponse->setValues(makePtr<StringMap>(values));
    settingsResponse->setModel(makePtr<String>(Json(model).dump()));

    auto response = makePtr<ActiveSettingChangedResponse>();
    response->setSettingsResponse(settingsResponse);
    *outResult = response.releasePtr();
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* /*neededMetadataTypes*/)
{
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    const auto response = new SettingsResponse();

    response->setValue("pluginSideTestSpinBox", "100");

    *outResult = response;
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

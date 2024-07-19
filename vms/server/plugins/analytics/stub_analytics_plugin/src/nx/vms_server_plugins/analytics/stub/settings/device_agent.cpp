// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <algorithm>
#include <unordered_set>

#include <nx/kit/utils.h>
#include <nx/sdk/helpers/active_setting_changed_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/vms_server_plugins/analytics/stub/utils.h>

#include "actions.h"
#include "active_settings_rules.h"
#include "settings_model.h"
#include "stub_analytics_plugin_settings_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

using nx::kit::Json;
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

    const std::string settingsModelType = settingValue(kSettingsModelSettings);

    std::string settingsModel;
    if (settingsModelType == kAlternativeSettingsModelOption)
    {
        settingsModel = kAlternativeSettingsModel;
    }
    else if (settingsModelType == kRegularSettingsModelOption)
    {
        const std::string languagePart = (settingValue(kCitySelector) == kGermanOption)
            ? kGermanCitiesSettingsModelPart
            : kEnglishCitiesSettingsModelPart;

        settingsModel = kRegularSettingsModelPart1
            + languagePart
            + kRegularSettingsModelPart2;
    }

    std::string parsingError;
    Json parsedSettingsModel = Json::parse(settingsModel, parsingError);
    std::map<std::string, std::string> settingsValues = currentSettings();
    processActiveSettings(&parsedSettingsModel, &settingsValues);

    settingsResponse->setModel(parsedSettingsModel.dump());
    settingsResponse->setValues(makePtr<StringMap>(settingsValues));
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
    const IActiveSettingChangedAction* activeSettingChangedAction) const
{
    NX_PRINT << "IActiveSettingChangedAction:";
    NX_PRINT << "{";
    NX_PRINT << "    \"activeSettingName\": "
        << nx::kit::utils::toString(activeSettingChangedAction->activeSettingName()) << ",";
    NX_PRINT << "    \"settingsModel\": "
        << nx::kit::utils::toString(activeSettingChangedAction->settingsModel()) << ",";
    NX_PRINT << "    \"settingsValues\":";
    dumpStringMap(/*prefix*/ "    ", /*appendix*/ ",",
        activeSettingChangedAction->settingsValues().get());
    NX_PRINT << "    \"params\":";
    dumpStringMap(/*prefix*/ "    ", /*appendix*/ "",
        activeSettingChangedAction->params().get());
    NX_PRINT << "}";
}

Json::array deduplicateArrayByPropertyName(const Json::array& array, const std::string& propertyName)
{
    std::unordered_set<std::string> uniquePropertyValues;
    Json::array deduplicatedArray;

    for (const Json& item: array)
    {
        if (!item.is_object())
        {
            deduplicatedArray.push_back(item);
            continue;
        }

        const auto propertyValue = item[propertyName];

        if (!propertyValue.is_string())
        {
            deduplicatedArray.push_back(item);
            continue;
        }

        const std::string propertyValueString = propertyValue.string_value();

        if (uniquePropertyValues.find(propertyValueString) == uniquePropertyValues.end())
        {
            deduplicatedArray.push_back(item);
            uniquePropertyValues.insert(propertyValueString);
        }
    }

    return deduplicatedArray;
}

void DeviceAgent::doGetSettingsOnActiveSettingChange(
    Result<const IActiveSettingChangedResponse*>* outResult,
    const IActiveSettingChangedAction* activeSettingChangedAction)
{
    if (NX_DEBUG_ENABLE_OUTPUT)
        dumpActiveSettingChangedAction(activeSettingChangedAction);

    using namespace nx::kit;

    std::string parseError;
    Json::object model = Json::parse(
        activeSettingChangedAction->settingsModel(), parseError).object_items();
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

    const std::string settingId(activeSettingChangedAction->activeSettingName());
    Json activeSettingsItems = (*activeSettingsSectionIt)[kItems];
    std::map<std::string, std::string> values = toStdMap(shareToPtr(
        activeSettingChangedAction->settingsValues()));

    m_activeSettingsBuilder.updateSettings(settingId, &activeSettingsItems, &values);

    Json::array updatedActiveSettingsItems = activeSettingsItems.array_items();

    updatedActiveSettingsItems = deduplicateArrayByPropertyName(updatedActiveSettingsItems, kName);

    Json::object updatedActiveSection = activeSettingsSectionIt->object_items();
    updatedActiveSection[kItems] = updatedActiveSettingsItems;
    *activeSettingsSectionIt = updatedActiveSection;
    model[kSections] = sections;

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

void DeviceAgent::processActiveSettings(
    nx::kit::Json* inOutSettingsModel,
    std::map<std::string, std::string>* inOutSettingsValues)
{
    Json::array sections = (*inOutSettingsModel)[kSections].array_items();
    auto activeSettingsSectionIt = std::find_if(sections.begin(), sections.end(),
        [](Json& section)
        {
            return section[kCaption].string_value() == kActiveSettingsSectionCaption;
        });

    if (activeSettingsSectionIt == sections.cend())
        return;

    Json activeSettingsItems = (*activeSettingsSectionIt)[kItems];
    std::vector<std::string> activeSettingNames;
    for (const Json& item: activeSettingsItems.array_items())
    {
        if (item[kIsActive].bool_value())
            activeSettingNames.push_back(item[kName].string_value());
    }

    for (const std::string& activeSettingName: activeSettingNames)
    {
        m_activeSettingsBuilder.updateSettings(
            activeSettingName, &activeSettingsItems, inOutSettingsValues);
    }

    Json::array updatedActiveSettingsItems = activeSettingsItems.array_items();
    Json::object updatedActiveSection = activeSettingsSectionIt->object_items();
    updatedActiveSection[kItems] = updatedActiveSettingsItems;
    *activeSettingsSectionIt = updatedActiveSection;

    Json::object updatedModel = inOutSettingsModel->object_items();
    updatedModel[kSections] = sections;

    *inOutSettingsModel = Json(updatedModel);
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

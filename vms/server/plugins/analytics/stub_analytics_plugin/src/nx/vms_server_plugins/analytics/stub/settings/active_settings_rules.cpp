// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "active_settings_rules.h"
#include "settings_model.h"

#include <string>
#include <algorithm>

#include <nx/kit/json.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_map.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

using namespace nx::sdk;
using namespace nx::kit;

const std::string kName = "name";
const std::string kRange = "range";

const std::string kAdditionalComboBoxSetting = R"json(
    {
        "type": "ComboBox",
        "name": ")json" + kAdditionalComboBoxId + R"json(",
        "caption": "Additional ComboBox",
        "defaultValue": "Value 1",
        "range":
        [
            "Value 1",
            "Value 2"
        ]
    }
)json";

const std::string kAdditionalCheckBoxSetting = R"json(
    {
        "type": "CheckBox",
        "name": ")json" + kAdditionalCheckBoxId + R"json(",
        "caption": "Active CheckBox",
        "defaultValue": false
    }
)json";

const std::string kAdditionalTextBoxSetting = R"json(
    {
        "type": "TextField",
        "name": ")json" + kActiveTextBoxId + R"json(",
        "caption": "Active TextBox",
        "defaultValue": "print \")json" + kShowAdditionalTextBoxValue + R"json(\""
    }
)json";

const std::string kAdditionalRadioButton = "Hide me";

// ------------------------------------------------------------------------------------------------

const std::map<
    ActiveSettingsBuilder::ActiveSettingKey,
    ActiveSettingsBuilder::ActiveSettingHandler> kActiveSettingsRules =
{
    {{kActiveComboBoxId, kShowAdditionalComboBoxValue}, showAdditionalComboBox},
    {{kActiveCheckBoxId, kShowAdditionalCheckBoxValue}, showAdditionalCheckBox},
    {{kActiveTextBoxId, kShowAdditionalTextBoxValue}, showAdditionalTextBox},
    {{kActiveRadioButtonGroupId, kShowAdditionalRadioButtonValue}, showAdditionalRadioButton},
    {{kActiveRadioButtonGroupId, kHideAdditionalRadioButtonValue}, hideAdditionalRadioButton}
};

const std::map<
    /*activeSettingId*/ std::string,
    ActiveSettingsBuilder::ActiveSettingHandler> kDefaultActiveSettingsRules =
{
    {kActiveComboBoxId, hideAdditionalComboBox},
    {kActiveCheckBoxId, hideAdditionalCheckBox},
    {kActiveTextBoxId, hideAdditionalTextBox}
};

// ------------------------------------------------------------------------------------------------

static Json::array::iterator findSetting(
    const std::string& settingName,
    nx::kit::Json::array* inOutSettings)
{
    auto findByName =
        [&](const nx::kit::Json& value)
        {
            return value[kName].string_value() == settingName;
        };

    return std::find_if(inOutSettings->begin(), inOutSettings->end(), findByName);
}

void showAdditionalSetting(
    Json* inOutModel,
    std::map<std::string, std::string>* inOutValues,
    const std::string activeSettingId,
    const std::string additionalSettingTemplate,
    const std::string additionalSettingValue)
{
    Json::array items = inOutModel->array_items();

    std::string error;
    Json newItem = Json::parse(additionalSettingTemplate, error);

    const auto settingIt = findSetting(activeSettingId, &items);
    if (settingIt != items.end())
        items.insert(settingIt + 1, newItem);

    *inOutModel = Json(items);
    const auto valueIt = inOutValues->find(kAdditionalComboBoxValue);
    if (valueIt == inOutValues->cend())
        inOutValues->emplace(kAdditionalComboBoxId, kAdditionalComboBoxValue);
}

void hideAdditionalSetting(
    Json* inOutModel,
    std::map<std::string, std::string>* inOutValues,
    const std::string additionalSettingId)
{
    Json::array items = inOutModel->array_items();
    const auto it = findSetting(additionalSettingId, &items);
    if (it != items.end())
        items.erase(it);

    *inOutModel = Json(items);
    inOutValues->erase(additionalSettingId);
}

void showAdditionalSettingOption(
    Json* inOutModel,
    std::map<std::string, std::string>* inOutValues,
    const std::string& activeSettingId,
    const std::string& additionalSettingOption)
{
    Json::array items = inOutModel->array_items();

    auto settingIt = findSetting(activeSettingId, &items);
    if (settingIt != items.end())
    {
        Json::object settingWithRange = settingIt->object_items();
        Json::array range = settingWithRange[kRange].array_items();
        range.push_back(additionalSettingOption);
        settingWithRange[kRange] = range;
        *settingIt = Json(settingWithRange);
    }

    *inOutModel = Json(items);
    const auto valueIt = inOutValues->find(kAdditionalComboBoxValue);
    if (valueIt == inOutValues->cend())
        inOutValues->emplace(kAdditionalComboBoxId, kAdditionalComboBoxValue);
}

void hideAdditionalSettingOption(
    Json* inOutModel,
    std::map<std::string, std::string>* inOutValues,
    const std::string& activeSettingId,
    const std::string& additionalSettingValue,
    const std::string& newAdditionalSettingValue)
{
    Json::array items = inOutModel->array_items();
    auto settingIt = findSetting(activeSettingId, &items);
    if (settingIt != items.end())
    {
        Json::object settingWithRange = settingIt->object_items();
        Json::array range = settingWithRange[kRange].array_items();

        const auto valueIt = std::find(range.cbegin(), range.cend(), additionalSettingValue);
        if (valueIt != range.cend())
            range.erase(valueIt);

        settingWithRange[kRange] = range;
        *settingIt = Json(settingWithRange);
    }

    *inOutModel = Json(items);
    (*inOutValues)[activeSettingId] = newAdditionalSettingValue;
}

void showAdditionalComboBox(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    showAdditionalSetting(
        inOutModel,
        inOutValues,
        kActiveComboBoxId,
        kAdditionalComboBoxSetting,
        kAdditionalComboBoxValue);
}

void hideAdditionalComboBox(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    hideAdditionalSetting(
        inOutModel,
        inOutValues,
        kAdditionalComboBoxId);
}

void showAdditionalCheckBox(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    showAdditionalSetting(
        inOutModel,
        inOutValues,
        kActiveCheckBoxId,
        kAdditionalCheckBoxSetting,
        kAdditionalCheckBoxValue);
}

void hideAdditionalCheckBox(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    hideAdditionalSetting(
        inOutModel,
        inOutValues,
        kAdditionalCheckBoxId);
}

void showAdditionalTextBox(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    showAdditionalSetting(
        inOutModel,
        inOutValues,
        kActiveTextBoxId,
        kAdditionalTextBoxSetting,
        kAdditionalTextBoxValue);
}

void hideAdditionalTextBox(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    hideAdditionalSetting(
        inOutModel,
        inOutValues,
        kAdditionalTextBoxId);
}

void showAdditionalRadioButton(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    showAdditionalSettingOption(
        inOutModel,
        inOutValues,
        kActiveRadioButtonGroupId,
        kAdditionalRadioButton);
}

void hideAdditionalRadioButton(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    hideAdditionalSettingOption(
        inOutModel,
        inOutValues,
        kActiveRadioButtonGroupId,
        kHideAdditionalRadioButtonValue,
        kDefaultActiveRadioButtonGroupValue);
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

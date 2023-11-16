// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "active_settings_rules.h"
#include "nx/kit/debug.h"
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
        "caption": "Additional CheckBox",
        "defaultValue": false
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
    {{kActiveRadioButtonGroupId, kShowAdditionalRadioButtonValue}, showAdditionalRadioButton},
    {{kActiveRadioButtonGroupId, kHideAdditionalRadioButtonValue}, hideAdditionalRadioButton},
    {{kActiveCheckBoxForValueSetChangeId, "true"}, addOptionalValueToComboBox},
    {{kActiveCheckBoxForValueSetChangeId, "false"}, removeOptionalValueToComboBox}
};

const std::map<
    /*activeSettingName*/ std::string,
    ActiveSettingsBuilder::ActiveSettingHandler> kDefaultActiveSettingsRules =
{
    {kActiveComboBoxId, hideAdditionalComboBox},
    {kActiveCheckBoxId, hideAdditionalCheckBox},
    {kActiveMaxValueId, updateMinMaxSpinBoxes},
    {kActiveMinValueId, updateMinMaxSpinBoxes},
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
    const std::string activeSettingName,
    const std::string additionalSettingTemplate,
    const std::string additionalSettingValue)
{
    Json::array items = inOutModel->array_items();

    std::string error;
    Json newItem = Json::parse(additionalSettingTemplate, error);
    const std::string newItemName = newItem[kName].string_value();

    const auto settingIt = findSetting(activeSettingName, &items);
    if (settingIt != items.end())
        items.insert(settingIt + 1, newItem);

    *inOutModel = Json(items);
    const auto valueIt = inOutValues->find(newItemName);
    if (valueIt == inOutValues->cend())
        inOutValues->emplace(newItemName, additionalSettingValue);
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
    const std::string& activeSettingName,
    const std::string& additionalSettingOption)
{
    Json::array items = inOutModel->array_items();

    auto settingIt = findSetting(activeSettingName, &items);
    if (settingIt != items.end())
    {
        Json::object settingWithRange = settingIt->object_items();
        Json::array range = settingWithRange[kRange].array_items();

        if (std::find(range.begin(), range.end(), additionalSettingOption) == range.cend())
        {
            range.push_back(additionalSettingOption);
            settingWithRange[kRange] = range;
            *settingIt = Json(settingWithRange);
        }
    }

    *inOutModel = Json(items);
    const auto valueIt = inOutValues->find(activeSettingName);
    if (valueIt == inOutValues->cend())
        inOutValues->emplace(activeSettingName, additionalSettingOption);
}

void hideAdditionalSettingOption(
    Json* inOutModel,
    std::map<std::string, std::string>* inOutValues,
    const std::string& activeSettingName,
    const std::string& additionalSettingValue,
    const std::string& newAdditionalSettingValue)
{
    Json::array items = inOutModel->array_items();
    auto settingIt = findSetting(activeSettingName, &items);
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
    (*inOutValues)[activeSettingName] = newAdditionalSettingValue;
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

void addOptionalValueToComboBox(Json* inOutModel, std::map<std::string, std::string>* /*inOutValues*/)
{
    Json::array items = inOutModel->array_items();
    auto comboBoxIt = findSetting(kComboBoxForValueSetChangeId, &items);
    if (!NX_KIT_ASSERT(comboBoxIt != items.end()))
        return;

    Json::object settingWithRange = comboBoxIt->object_items();

    settingWithRange[kRange] = Json::array{
        kComboBoxForValueSetChangeValuePermanent,
        kComboBoxForValueSetChangeValueOptional,
    };

    *comboBoxIt = Json(settingWithRange);
    *inOutModel = Json(items);
}

void removeOptionalValueToComboBox(Json* inOutModel, std::map<std::string, std::string>* inOutValues)
{
    Json::array items = inOutModel->array_items();
    auto comboBoxIt = findSetting(kComboBoxForValueSetChangeId, &items);
    if (!NX_KIT_ASSERT(comboBoxIt != items.end()))
        return;

    Json::object settingWithRange = comboBoxIt->object_items();

    settingWithRange[kRange] = Json::array{
        kComboBoxForValueSetChangeValuePermanent,
    };

    *comboBoxIt = Json(settingWithRange);
    *inOutModel = Json(items);

    (*inOutValues)[kComboBoxForValueSetChangeId] = kComboBoxForValueSetChangeValuePermanent;
}

void updateMinMaxSpinBoxes(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues)
{
    Json::array items = inOutModel->array_items();
    auto minIt = findSetting(kActiveMinValueId, &items);
    auto maxIt = findSetting(kActiveMaxValueId, &items);

    if (minIt == items.end() || maxIt == items.end())
        return;

    const std::string minValueStr = (*inOutValues)[kActiveMinValueId];
    const std::string maxValueStr = (*inOutValues)[kActiveMaxValueId];

    if (minValueStr.empty() || maxValueStr.empty())
        return;

    const int minValue = std::stoi(minValueStr);
    const int maxValue = std::stoi(maxValueStr);

    auto setMinMax =
        [](Json::array::iterator spinBoxIt, int min, int max)
        {
            Json::object spinBox = spinBoxIt->object_items();
            spinBox[kMinValue] = min;
            spinBox[kMaxValue] = max;
            *spinBoxIt = Json(spinBox);
        };

    setMinMax(minIt, (*minIt)[kMinValue].int_value(), maxValue);
    setMinMax(maxIt, minValue, (*maxIt)[kMaxValue].int_value());

    *inOutModel = Json(items);
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

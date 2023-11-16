// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>

#include "active_settings_builder.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

extern const std::map<
    ActiveSettingsBuilder::ActiveSettingKey,
    ActiveSettingsBuilder::ActiveSettingHandler> kActiveSettingsRules;

extern const std::map<
    /*activeSettingName*/ std::string,
    ActiveSettingsBuilder::ActiveSettingHandler> kDefaultActiveSettingsRules;

void showAdditionalComboBox(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void hideAdditionalComboBox(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void showAdditionalCheckBox(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void hideAdditionalCheckBox(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void showAdditionalRadioButton(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void hideAdditionalRadioButton(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void updateMinMaxSpinBoxes(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void addOptionalValueToComboBox(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

void removeOptionalValueToComboBox(
    nx::kit::Json* inOutModel,
    std::map<std::string, std::string>* inOutValues);

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "active_settings_builder.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

using namespace nx::sdk;

void ActiveSettingsBuilder::addRule(
    const std::string& activeSettingName,
    const std::string& activeSettingValue,
    ActiveSettingHandler activeSettingHandler)
{
    ActiveSettingKey key{activeSettingName, activeSettingValue};
    m_rules[key] = activeSettingHandler;
}

void ActiveSettingsBuilder::addDefaultRule(
    const std::string& activeSettingName,
    ActiveSettingHandler activeSettingHandler)
{
    m_defaultRules[activeSettingName] = activeSettingHandler;
}

void ActiveSettingsBuilder::updateSettings(
    const std::string& activeSettingName,
    nx::kit::Json* inOutSettingsModel,
    std::map<std::string, std::string>* inOutSettingsValues) const
{
    ActiveSettingKey key{activeSettingName, (*inOutSettingsValues)[activeSettingName]};

    auto rulesIt = m_rules.find(key);
    auto defaultRulesIt = m_defaultRules.find(activeSettingName);

    if (rulesIt != m_rules.cend())
        rulesIt->second(inOutSettingsModel, inOutSettingsValues);
    else if (defaultRulesIt != m_defaultRules.cend())
        defaultRulesIt->second(inOutSettingsModel, inOutSettingsValues);
}

bool ActiveSettingsBuilder::ActiveSettingKey::operator<(const ActiveSettingKey& other) const
{
    if (activeSettingName != other.activeSettingName)
        return activeSettingName < other.activeSettingName;

    if (activeSettingValue != other.activeSettingValue)
        return activeSettingValue < other.activeSettingValue;

    return false;
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

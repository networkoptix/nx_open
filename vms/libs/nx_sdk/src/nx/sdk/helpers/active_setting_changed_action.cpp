// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "active_setting_changed_action.h"

namespace nx::sdk {

void ActiveSettingChangedAction::setActiveSettingId(std::string value)
{
    m_activeSettingName = std::move(value);
}

void ActiveSettingChangedAction::setSettingsModel(std::string value)
{
    m_settingsModel = std::move(value);
}

void ActiveSettingChangedAction::setSettingsValues(Ptr<const StringMap> value)
{
    m_settingsValues = value;
}

void ActiveSettingChangedAction::setParams(Ptr<const StringMap> value)
{
    m_params = value;
}

const char* ActiveSettingChangedAction::activeSettingName() const
{
    return m_activeSettingName.c_str();
}

const char* ActiveSettingChangedAction::settingsModel() const
{
    return m_settingsModel.c_str();
}

const IStringMap* ActiveSettingChangedAction::getSettingsValues() const
{
    return shareToPtr(m_settingsValues).releasePtr();
}

const IStringMap* ActiveSettingChangedAction::getParams() const
{
    return shareToPtr(m_params).releasePtr();
}

} // namespace nx::sdk

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "active_setting_changed_response.h"

namespace nx::sdk {

void ActiveSettingChangedResponse::setActionResponse(Ptr<const ActionResponse> value)
{
    m_actionResponse = value;
}

void ActiveSettingChangedResponse::setSettingsResponse(
    Ptr<const SettingsResponse> value)
{
    m_settingsResponse = value;
}

const IActionResponse* ActiveSettingChangedResponse::getActionResponse() const
{
    return shareToPtr(m_actionResponse).releasePtr();
}

const ISettingsResponse* ActiveSettingChangedResponse::getSettingsResponse() const
{
    return shareToPtr(m_settingsResponse).releasePtr();
}

} // namespace nx::sdk

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/helpers/action_response.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/i_active_setting_changed_response.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk {

class ActiveSettingChangedResponse: public RefCountable<IActiveSettingChangedResponse>
{
public:
    ActiveSettingChangedResponse() = default;

    void setActionResponse(Ptr<const ActionResponse> value);
    void setSettingsResponse(Ptr<const SettingsResponse> value);

protected:
    virtual const IActionResponse* getActionResponse() const override;
    virtual const ISettingsResponse* getSettingsResponse() const override;

private:
    Ptr<const ActionResponse> m_actionResponse;
    Ptr<const SettingsResponse> m_settingsResponse;
};

} // namespace nx::sdk

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/i_active_setting_changed_action.h>

namespace nx::sdk {

class ActiveSettingChangedAction: public RefCountable<IActiveSettingChangedAction>
{
public:
    ActiveSettingChangedAction() = default;

    void setActiveSettingId(std::string value);
    void setSettingsModel(std::string value);
    void setSettingsValues(Ptr<const StringMap> value);
    void setParams(Ptr<const StringMap> value);

    virtual const char* activeSettingName() const override;
    virtual const char* settingsModel() const override;

protected:
    virtual const IStringMap* getSettingsValues() const override;
    virtual const IStringMap* getParams() const override;

private:
    std::string m_activeSettingName;
    std::string m_settingsModel;
    Ptr<const StringMap> m_settingsValues;
    Ptr<const StringMap> m_params;
};

} // namespace nx::sdk

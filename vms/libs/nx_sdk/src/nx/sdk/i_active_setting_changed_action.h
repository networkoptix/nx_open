// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk {

/**
 * Data sent from the Client to the plugin when the user changes the state of an Active setting.
 */
class IActiveSettingChangedAction: public Interface<IActiveSettingChangedAction>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IActiveSettingChangedAction"); }

    /**
     * @return Name of a setting which has triggered this notification.
     */
    virtual const char* activeSettingName() const = 0;

    /**
     * @return Model of settings the Active setting has been triggered on. Never null.
     */
    virtual const char* settingsModel() const = 0;

    /** Called by settingsValues() */
    protected: virtual const IStringMap* getSettingsValues() const = 0;
    /**
     * @return Values currently set in the GUI. Never null.
     */
    public: Ptr<const IStringMap> settingsValues() const { return Ptr(getSettingsValues()); }

    /** Called by params() */
    protected: virtual const IStringMap* getParams() const = 0;
    /**
     * If the Active setting defines params in the Manifest, contains the array of their
     * values after they are filled by the user via the Client form. Otherwise, null.
     */
    public: Ptr<const IStringMap> params() const { return Ptr(getParams()); }
};
using IActiveSettingChangedAction0 = IActiveSettingChangedAction;

} // namespace nx::sdk

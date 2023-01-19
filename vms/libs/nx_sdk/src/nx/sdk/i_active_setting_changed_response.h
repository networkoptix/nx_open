// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/ptr.h>

#include "i_action_response.h"
#include "i_settings_response.h"

namespace nx::sdk {

/**
 * Data returned from the plugin when the user changes some setting in the dialog.
 */
class IActiveSettingChangedResponse: public Interface<IActiveSettingChangedResponse>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IActiveSettingChangedResponse"); }

    /** Called by actionResponse() */
    protected: virtual const IActionResponse* getActionResponse() const = 0;
    /**
     * @return Data for interacting with the user, or null if such interaction is not needed.
     */
    public: Ptr<const IActionResponse> actionResponse() const
    {
        return Ptr(getActionResponse());
    }

    /** Called by settingsResponse() */
    protected: virtual const ISettingsResponse* getSettingsResponse() const = 0;
    /**
     * A combination of optional individual setting errors, optional new setting values in case
     * they were adjusted, and an optional new Settings Model. Can be null if none of the above
     * items are present.
     */
    public: Ptr<const ISettingsResponse> settingsResponse() const
    {
        return Ptr(getSettingsResponse());
    }
};
using IActiveSettingChangedResponse0 = IActiveSettingChangedResponse;

} // namespace nx::sdk

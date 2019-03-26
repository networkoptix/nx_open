#pragma once

#include <optional>

#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/client/desktop/common/redux/abstract_redux_state.h>

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API ServerSettingsDialogState: AbstractReduxState
{
    bool isOnline = false;

    struct PluginsInformation
    {
        nx::vms::api::PluginModuleDataList modules;
        bool loading = false;
        int current = -1;
    };

    PluginsInformation plugins;

    std::optional<nx::vms::api::PluginModuleData> currentPlugin() const
    {
        if (plugins.current < 0 || plugins.current > plugins.modules.size())
            return {};

        return plugins.modules[plugins.current];
    }
};

} // namespace nx::vms::client::desktop

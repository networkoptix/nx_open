#pragma once

#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/client/desktop/common/redux/abstract_redux_state.h>

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API ServerSettingsDialogState: AbstractReduxState
{
    struct PluginsInformation
    {
        nx::vms::api::PluginModuleDataList modules;
        bool loading = false;
        QString currentLibraryName;
    };

    PluginsInformation plugins;
};

} // namespace nx::vms::client::desktop

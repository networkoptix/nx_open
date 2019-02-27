#include "server_settings_dialog_state_reducer.h"

#include <core/resource/media_server_resource.h>

namespace nx::vms::client::desktop {

using State = ServerSettingsDialogState;
using Server = QnMediaServerResourcePtr;

State ServerSettingsDialogStateReducer::loadServer(State state, const Server& server)
{
    state.plugins = {};
    return state;
}

State ServerSettingsDialogStateReducer::setPluginModules(
    State state, const nx::vms::api::PluginModuleDataList& value)
{
    state.plugins.modules = value;
    // TODO: #vkutin Filter out invalid results to make libraryName an unique key?

    if (!state.plugins.currentLibraryName.isEmpty())
    {
        const auto iter = std::find_if(state.plugins.modules.cbegin(), state.plugins.modules.cend(),
            [&state](const nx::vms::api::PluginModuleData& data)
            {
                return state.plugins.currentLibraryName == data.libraryName;
            });

        if (iter == state.plugins.modules.cend())
            state.plugins.currentLibraryName = QString();
    }

    return state;
}

State ServerSettingsDialogStateReducer::setCurrentPluginLibraryName(
    State state, const QString& value)
{
    state.plugins.currentLibraryName = value;
    return state;
}

State ServerSettingsDialogStateReducer::setPluginsInformationLoading(State state, bool value)
{
    state.plugins.loading = value;
    return state;
}

} // namespace nx::vms::client::desktop

#include "server_settings_dialog_state_reducer.h"

#include <core/resource/media_server_resource.h>

namespace nx::vms::client::desktop {

using State = ServerSettingsDialogState;
using Server = QnMediaServerResourcePtr;

namespace {

int pluginIndex(const State& state, const QString& libraryName)
{
    if (libraryName.isEmpty())
        return -1;

    const auto iter = std::find_if(state.plugins.modules.cbegin(), state.plugins.modules.cend(),
        [libraryName](const nx::vms::api::PluginModuleData& data)
        {
            return data.libraryName == libraryName;
        });

    return iter != state.plugins.modules.cend()
        ? std::distance(state.plugins.modules.cbegin(), iter)
        : -1;
}

} // namespace

State ServerSettingsDialogStateReducer::loadServer(State state, const Server& server)
{
    state.isOnline = false;
    state.plugins = {};
    return state;
}

State ServerSettingsDialogStateReducer::setOnline(State state, bool value)
{
    state.isOnline = value;
    return state;
}

State ServerSettingsDialogStateReducer::setPluginModules(
    State state, const nx::vms::api::PluginModuleDataList& value)
{
    const auto currentLibraryName = state.plugins.current > 0
        ? state.plugins.modules[state.plugins.current].libraryName
        : QString();

    state.plugins.modules = value;
    // TODO: #vkutin Filter out invalid results to make libraryName an unique key?

    state.plugins.current = pluginIndex(state, currentLibraryName);
    if (state.plugins.current < 0 && !state.plugins.modules.empty())
        state.plugins.current = 0;

    return state;
}

State ServerSettingsDialogStateReducer::selectCurrentPlugin(
    State state, const QString& libraryName)
{
    state.plugins.current = pluginIndex(state, libraryName);
    return state;
}

State ServerSettingsDialogStateReducer::setPluginsInformationLoading(State state, bool value)
{
    state.plugins.loading = value;
    return state;
}

} // namespace nx::vms::client::desktop

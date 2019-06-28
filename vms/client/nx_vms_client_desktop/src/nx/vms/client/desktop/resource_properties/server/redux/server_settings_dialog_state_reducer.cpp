#include "server_settings_dialog_state_reducer.h"

#include <core/resource/media_server_resource.h>

#include <nx/utils/string.h>

namespace nx::vms::client::desktop {

using State = ServerSettingsDialogState;
using Server = QnMediaServerResourcePtr;

namespace {

int pluginIndex(const State& state, const QString& libraryFilename)
{
    if (libraryFilename.isEmpty())
        return -1;

    const auto iter = std::find_if(state.plugins.modules.cbegin(), state.plugins.modules.cend(),
        [libraryFilename](const nx::vms::api::PluginInfo& data)
        {
            return data.libraryFilename == libraryFilename;
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
    State state, const nx::vms::api::PluginInfoList& value)
{
    using PluginInfo = nx::vms::api::PluginInfo;

    const auto currentLibraryName = state.plugins.current > 0
        ? state.plugins.modules[state.plugins.current].libraryFilename
        : QString();

    state.plugins.modules.clear();

    // Ensure uniqueness of libraryFilename.
    QSet<QString> usedNames;
    for (const auto& plugin: value)
    {
        if (!NX_ASSERT(!usedNames.contains(plugin.libraryFilename)))
            continue;

        usedNames.insert(plugin.libraryFilename);
        state.plugins.modules.push_back(plugin);
    }

    // Sort plugins by loaded state, then by name.
    std::sort(state.plugins.modules.begin(), state.plugins.modules.end(),
        [](const PluginInfo& l, const PluginInfo& r)
        {
            if (l.status == PluginInfo::Status::loaded && r.status != PluginInfo::Status::loaded)
                return true;
            if (r.status == PluginInfo::Status::loaded && l.status != PluginInfo::Status::loaded)
                return false;

            const int code = nx::utils::naturalStringCompare(l.name, r.name, Qt::CaseInsensitive);
            if (code < 0)
                return true;
            if (code > 0)
                return false;

            return l.libraryFilename < r.libraryFilename;
        });

    state.plugins.current = pluginIndex(state, currentLibraryName);
    if (state.plugins.current < 0 && !state.plugins.modules.empty())
        state.plugins.current = 0;

    return state;
}

State ServerSettingsDialogStateReducer::selectCurrentPlugin(
    State state, const QString& libraryFilename)
{
    state.plugins.current = pluginIndex(state, libraryFilename);
    return state;
}

State ServerSettingsDialogStateReducer::setPluginsInformationLoading(State state, bool value)
{
    state.plugins.loading = value;
    return state;
}

} // namespace nx::vms::client::desktop

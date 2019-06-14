#include "plugin_home_dir_utility_provider.h"

#include <nx/sdk/helpers/string.h>
#include <plugins/plugin_manager.h>

namespace nx::vms::server::plugins {

using namespace nx::sdk;

const nx::sdk::IString* PluginHomeDirUtilityProvider::homeDir(const nx::sdk::IPlugin* plugin) const
{
    if (!plugin)
    {
        NX_ERROR(this, "Plugin called homeDir(nullptr)");
        return new nx::sdk::String("");
    }

    if (const auto pluginInfo = m_pluginManager->pluginInfo(plugin))
        return new nx::sdk::String(pluginInfo->homeDir.toStdString());

    return new nx::sdk::String(""); //< The error is already logged.
}

} // namespace nx::vms::server::plugins

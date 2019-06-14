#pragma once

#include <map>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_plugin_home_dir_utility_provider.h>

class PluginManager;

namespace nx::vms::server::plugins {

class PluginHomeDirUtilityProvider:
    public nx::sdk::RefCountable<nx::sdk::IPluginHomeDirUtilityProvider>
{
public:
    PluginHomeDirUtilityProvider(PluginManager* pluginManager): m_pluginManager(pluginManager) {}

    virtual const nx::sdk::IString* homeDir(const nx::sdk::IPlugin* plugin) const override;

private:
    PluginManager* const m_pluginManager;
};

} // namespace nx::vms::server::plugins

#pragma once

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_utility_provider.h>
#include <nx/vms/server/plugins/time_utility_provider.h>
#include <nx/vms/server/plugins/plugin_home_dir_utility_provider.h>

class PluginManager;

namespace nx::vms::server::plugins {

class UtilityProvider: public nx::sdk::RefCountable<nx::sdk::IUtilityProvider>
{
public:
    UtilityProvider(PluginManager* pluginManager): m_pluginManager(pluginManager) {}

    virtual IRefCountable* queryInterface(InterfaceId id) override
    {
        if (const auto ptr = IUtilityProvider::queryInterface(id))
            return ptr;
        if (const auto ptr = m_timeUtilityProvider->queryInterface(id))
            return ptr;
        if (const auto ptr = m_pluginHomeDirUtilityProvider->queryInterface(id))
            return ptr;
        return nullptr;
    }

private:
    PluginManager* const m_pluginManager;

    nx::sdk::Ptr<TimeUtilityProvider> m_timeUtilityProvider{new TimeUtilityProvider()};

    nx::sdk::Ptr<PluginHomeDirUtilityProvider> m_pluginHomeDirUtilityProvider{
        new PluginHomeDirUtilityProvider(m_pluginManager)};
};

} // namespace nx::vms::server::plugins

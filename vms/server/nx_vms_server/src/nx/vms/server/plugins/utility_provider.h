#pragma once

#include <nx/sdk/i_utility_provider.h>
#include <nx/sdk/helpers/ref_countable.h>

class PluginManager;

namespace nx::vms::server::plugins {

class UtilityProvider: public nx::sdk::RefCountable<nx::sdk::IUtilityProvider>
{
public:
    UtilityProvider(PluginManager* pluginManager): m_pluginManager(pluginManager) {}

    virtual IRefCountable* queryInterface(InterfaceId id) override;

    virtual int64_t vmsSystemTimeSinceEpochMs() const override;

    virtual const nx::sdk::IString* homeDir(const nx::sdk::IPlugin* plugin) const override;

private:
    PluginManager* const m_pluginManager;
};

} // namespace nx::vms::server::plugins

#pragma once

#include <nx/sdk/i_utility_provider.h>
#include <nx/sdk/helpers/ref_countable.h>

class PluginManager;
namespace nx::sdk { class IPlugin; }

namespace nx::vms::server::plugins {

class UtilityProvider: public nx::sdk::RefCountable<nx::sdk::IUtilityProvider>
{
public:
    UtilityProvider(PluginManager* pluginManager, const nx::sdk::IPlugin* plugin);

    virtual int64_t vmsSystemTimeSinceEpochMs() const override;

protected:
    virtual IRefCountable* queryInterface(const InterfaceId* id) override;
    virtual const nx::sdk::IString* getHomeDir() const override;
    virtual const nx::sdk::IString* getServerSdkVersion() const override;

private:
    PluginManager* const m_pluginManager; /**< Never null (asserted in the constructor). */
    const nx::sdk::IPlugin* const m_plugin; /**< Can be null for old SDK plugins. */
};

} // namespace nx::vms::server::plugins

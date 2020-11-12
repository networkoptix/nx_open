#pragma once

#include <nx/sdk/analytics/helpers/engine.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class Plugin;

class Engine:
    public nx::sdk::analytics::Engine
{
public:
    explicit Engine(Plugin& plugin);

    Plugin& plugin() const;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

protected:
    virtual std::string manifestString() const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;

    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    Plugin& m_plugin;
};

} // namespace nx::vms_server_plugins::analytics::vivotek

#pragma once

#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/helpers/engine.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class Engine: public nx::sdk::analytics::Engine
{
public:
    explicit Engine(nx::sdk::Ptr<nx::sdk::IUtilityProvider> utilityProvider);

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

protected:
    virtual std::string manifestString() const override;

    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    const nx::sdk::Ptr<nx::sdk::IUtilityProvider> m_utilityProvider;
};

} // namespace nx::vms_server_plugins::analytics::vivotek

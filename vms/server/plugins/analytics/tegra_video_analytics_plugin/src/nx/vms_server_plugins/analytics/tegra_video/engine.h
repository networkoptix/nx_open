#pragma once

#include <nx/sdk/analytics/helpers/engine.h>

namespace nx { namespace sdk { namespace analytics { class Plugin; }}}

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

    virtual std::string manifestString() const override;

private:
    nx::sdk::analytics::Plugin* const m_plugin;
};

} // namespace tegra_video
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/result_aliases.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::IPlugin* plugin);

    virtual nx::sdk::analytics::MutableDeviceAgentResult obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo) override;

protected:
    virtual std::string manifestString() const override;
};

} // namespace tegra_video
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::IPlugin* plugin);

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo, nx::sdk::IError* outError) override;

protected:
    virtual std::string manifest() const override;
};

} // namespace tegra_video
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

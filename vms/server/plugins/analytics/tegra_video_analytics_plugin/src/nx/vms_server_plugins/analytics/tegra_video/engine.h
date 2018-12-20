#pragma once

#include <nx/sdk/analytics/common/engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

class Engine: public nx::sdk::analytics::common::Engine
{
public:
    Engine(nx::sdk::analytics::IPlugin* plugin);

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

protected:
    virtual std::string manifest() const override;
};

} // namespace tegra_video
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

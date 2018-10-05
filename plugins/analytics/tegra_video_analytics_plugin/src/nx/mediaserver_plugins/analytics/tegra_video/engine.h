#pragma once

#include <nx/sdk/analytics/common_engine.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace tegra_video {

class Engine: public nx::sdk::analytics::CommonEngine
{
public:
    Engine();

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

protected:
    virtual std::string manifest() const override;
};

} // namespace tegra_video
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

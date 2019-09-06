#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/analytics/helpers/plugin.h>

#include "tegra_video_analytics_plugin_ini.h"
#include "device_agent.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT),
    m_plugin(plugin)
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/1 + R"json(
    "eventTypes": [
        {
            "id": "nx.tegraVideo.humanEnteredTheArea",
            "name": "Human entered the area."
        },
        {
            "id": "nx.tegraVideo.humanLeftTheArea",
            "name": "Human left the area"
        }
    ],
    "objectTypes": [
        {
            "id": "nx.tegraVideo.car",
            "name": "Car"
        },
        {
            "id": "nx.tegraVideo.human",
            "name": "Human"
        }
    ]
}
)json";
}

} // namespace tegra_video
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.tegra_video",
    "name": "TegraVideo analytics plugin",
    "description": "Supports video analytics implemented using NVIDIA Tegra Multimedia API for Tegra (Jetson) AI platforms",
    "version": "1.0.0",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kPluginManifest,
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::tegra_video::Engine(plugin);
        });
}

} // extern "C"

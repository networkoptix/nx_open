#include "engine.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/helpers/plugin.h>

#include "tegra_video_analytics_plugin_ini.h"
#include "device_agent.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(IPlugin* plugin): nx::sdk::analytics::Engine(plugin, NX_DEBUG_ENABLE_OUTPUT)
{
}

IDeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* /*deviceInfo*/, Error* /*outError*/)
{
    return new DeviceAgent(this);
}

std::string Engine::manifest() const
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

static const std::string kLibName = "tegra_video_analytics_plugin";
static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.tegra_video",
    "name": "TegraVideo analytics plugin",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::tegra_video::Engine(plugin);
        });
}

} // extern "C"

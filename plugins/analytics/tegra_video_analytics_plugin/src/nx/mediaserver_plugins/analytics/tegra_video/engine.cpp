#include "engine.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>

#include "tegra_video_analytics_plugin_ini.h"
#include "device_agent.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace tegra_video {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(): CommonEngine(
    "Tegra Video Analytics Plugin", "tegra_video_analytics_plugin", NX_DEBUG_ENABLE_OUTPUT)
{
}

nx::sdk::analytics::DeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* /*deviceInfo*/, Error* /*outError*/)
{
    return new DeviceAgent(this);
}

std::string Engine::manifest() const
{
    return R"json(
        {
            "pluginId": "nx.tegraVideo",
            "pluginName": {
                "value": ")json" + std::string(name()) + R"json(",
                "localization": {
                    "ru_RU": "Tegra Video analytics engine Plugin (stub for Russian text)"
                }
            },
            "outputEventTypes": [
                {
                    "id": "nx.tegraVideo.humanEnteredTheArea",
                    "name": {
                        "value": "Human entered the area.",
                        "localization": {
                            "ru_RU": "Chelovek voznik (stub for Russian text)"
                        }
                    }
                },
                {
                    "id": "nx.tegraVideo.humanLeftTheArea",
                    "name": {
                        "value": "Human left the area",
                        "localization": {
                            "ru_RU": "Chelovek svalil (stub for Russian text)"
                        }
                    }
                }
            ],
            "outputObjectTypes": [
                {
                    "id": "nx.tegraVideo.car",
                    "name": {
                        "value": "Car",
                        "localization": {
                            "ru_RU": "Mashina (stub for Russian text)"
                        }
                    }
                },
                {
                    "id": "nx.tegraVideo.human",
                    "name": {
                        "value": "Human",
                        "localization": {
                            "ru_RU": "Chelovek (stub for Russian text)"
                        }
                    }
                }
            ]
        }
    )json";
}

} // namespace tegra_video
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsEngine()
{
    return new nx::mediaserver_plugins::analytics::tegra_video::Engine();
}

} // extern "C"

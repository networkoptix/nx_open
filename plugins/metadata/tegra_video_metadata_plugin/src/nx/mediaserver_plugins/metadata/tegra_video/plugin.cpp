#include "plugin.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>

#include "tegra_video_metadata_plugin_ini.h"
#include "camera_manager.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin(): CommonPlugin(
    "Tegra Video metadata plugin", "tegra_video_metadata_plugin", NX_DEBUG_ENABLE_OUTPUT)
{
}

nx::sdk::metadata::CameraManager* Plugin::obtainCameraManager(
    const CameraInfo* /*cameraInfo*/, Error* /*outError*/)
{
    return new CameraManager(this);
}

std::string Plugin::capabilitiesManifest() const
{
    return R"json(
        {
            "pluginId": "nx.tegraVideo",
            "pluginName": {
                "value": "Tegra Video Metadata Plugin",
                "localization": {
                    "ru_RU": "Tegra Video Metadata Plugin (stub for Russian text)"
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
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::tegra_video::Plugin();
}

} // extern "C"

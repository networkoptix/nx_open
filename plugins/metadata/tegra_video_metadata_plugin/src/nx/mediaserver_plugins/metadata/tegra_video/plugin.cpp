#include "plugin.h"

#define NX_PRINT_PREFIX printPrefix()
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
    const CameraInfo& /*cameraInfo*/, Error* /*outError*/)
{
    return new CameraManager(this);
}

std::string Plugin::capabilitiesManifest() const
{
    return R"json(
        {
            "driverId": "{B14A8D7B-8009-4D38-A60D-04139345432E}",
            "driverName": {
                "value": "Tegra Video Driver",
                "localization": {
                    "ru_RU": "Tegra Video driver (translated to Russian)"
                }
            },
            "outputEventTypes": [
                {
                    "typeId": "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                    "name": {
                        "value": "Human entered the area.",
                        "localization": {
                            "ru_RU": "Chelovek voznik (stub for Russian text)"
                        }
                    }
                },
                {
                    "typeId": "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}",
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
                    "typeId": "{58AE392F-8516-4B27-AEE1-311139B5A37A}",
                    "name": {
                        "value": "Car",
                        "localization": {
                            "ru_RU": "Mashina (stub for Russian text)"
                        }
                    }
                },
                {
                    "typeId": "{3778A599-FB60-47E9-8EC6-A9949E8E0AE7}",
                    "name": {
                        "value": "Human",
                        "localization": {
                            "ru_RU": "Chelovek (stub for Russian text)"
                        }
                    }
                }
            ],

            "options": "needDeepCopyOfVideoFrames"
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

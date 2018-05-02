#pragma once

#include <nx/sdk/metadata/common_plugin.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

class Plugin: public nx::sdk::metadata::CommonPlugin
{
public:
    Plugin();

    virtual nx::sdk::metadata::CameraManager* obtainCameraManager(
        const nx::sdk::CameraInfo& cameraInfo, nx::sdk::Error* outError) override;

protected:
    virtual std::string capabilitiesManifest() const override;
};

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

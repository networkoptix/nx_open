#pragma once

#include <nx/sdk/metadata/common_plugin.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

class Plugin: public sdk::metadata::CommonPlugin
{
public:
    Plugin(): CommonPlugin("Stub metadata plugin") {}

    virtual nx::sdk::metadata::CameraManager* obtainCameraManager(
        const nx::sdk::CameraInfo& cameraInfo, nx::sdk::Error* outError) override;

protected:
    virtual std::string capabilitiesManifest() const override;
};

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

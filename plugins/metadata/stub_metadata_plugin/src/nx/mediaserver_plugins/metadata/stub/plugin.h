#pragma once

#include <nx/sdk/metadata/common_plugin.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

class Plugin: public sdk::metadata::CommonPlugin
{
public:
    Plugin();

    virtual nx::sdk::metadata::CameraManager* obtainCameraManager(
        const nx::sdk::CameraInfo& cameraInfo, nx::sdk::Error* outError) override;

protected:
    virtual std::string capabilitiesManifest() const override;

    virtual void settingsChanged() override;

    virtual void executeAction(
        const std::string& actionId,
        nxpl::NX_GUID objectId,
        nxpl::NX_GUID cameraId,
        int64_t timestampUs,
        const std::map<std::string, std::string>& params,
        std::string* outActionUrl,
        std::string* outMessageToUser,
        nx::sdk::Error* error) override;
};

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

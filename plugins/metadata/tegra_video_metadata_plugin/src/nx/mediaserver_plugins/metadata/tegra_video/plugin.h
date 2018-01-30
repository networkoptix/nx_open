#pragma once

#include <nx/sdk/metadata/simple_plugin.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

class Plugin: public nx::sdk::metadata::SimplePlugin
{
public:
    Plugin(): SimplePlugin("Tegra Video metadata plugin") {}

    virtual nx::sdk::metadata::AbstractMetadataManager* managerForResource(
        const nx::sdk::ResourceInfo& resourceInfo,
        nx::sdk::Error* outError) override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;
};

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

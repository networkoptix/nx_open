#pragma once

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class StubMetadataPlugin:
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataPlugin>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;

    virtual void setSettings( const nxpl::Setting* settings, int count ) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual void setLocale(const char* locale) override;

    virtual nx::sdk::metadata::AbstractMetadataManager* obtainManagerForCamera(
        const nx::sdk::CameraInfo& cameraInfo,
        nx::sdk::Error* outError) override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx

#pragma once

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class Plugin: public nxpt::CommonRefCounter<nx::sdk::metadata::Plugin>
{
public:
    Plugin();
    virtual ~Plugin() override;

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

    virtual const char* name() const override;

    virtual void setSettings( const nxpl::Setting* settings, int count ) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual void setLocale(const char* locale) override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;

    virtual nx::sdk::metadata::CameraManager* obtainCameraManager(
        const nx::sdk::CameraInfo& cameraInfo, nx::sdk::Error* outError) override;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

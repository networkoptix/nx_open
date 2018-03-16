#pragma once

#include <vector>
#include <string>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>

#include <nx/mediaserver_plugins/metadata/deepstream/object_class_description.h>

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

    virtual void executeAction(nx::sdk::metadata::Action*, nx::sdk::Error*) override;

    std::vector<ObjectClassDescription> objectClassDescritions() const;

private:
    std::vector<ObjectClassDescription> loadObjectClasses() const;
    std::vector<std::string> loadLabels(const std::string& labelFilePath) const;
    std::vector<nxpl::NX_GUID> loadClassGuids(const std::string& guidsFilePath) const;

    std::string buildManifestObectTypeString(const ObjectClassDescription& description) const;

private:
    mutable std::vector<ObjectClassDescription> m_objectClassDescritions;
    mutable std::string m_manifest;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

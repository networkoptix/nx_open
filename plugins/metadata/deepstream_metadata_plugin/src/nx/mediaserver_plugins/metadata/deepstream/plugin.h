#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>

#include <nx/mediaserver_plugins/metadata/deepstream/default/object_class_description.h>

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

    std::chrono::microseconds currentTimeUs() const;

private:
    std::vector<ObjectClassDescription> loadObjectClasses() const;
    std::vector<std::string> loadLabels(const std::string& labelFilePath) const;
    std::vector<nxpl::NX_GUID> loadClassGuids(const std::string& guidsFilePath) const;

    std::string buildManifestObectTypeString(const ObjectClassDescription& description) const;

private:
    mutable std::vector<ObjectClassDescription> m_objectClassDescritions;
    mutable std::string m_manifest;
    std::unique_ptr<
        nxpl::TimeProvider,
        std::function<void(nxpl::TimeProvider*)>> m_timeProvider;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

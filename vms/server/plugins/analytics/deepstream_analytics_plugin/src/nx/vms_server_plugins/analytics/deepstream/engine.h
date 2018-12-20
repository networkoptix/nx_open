#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/common/plugin.h>

#include <nx/vms_server_plugins/analytics/deepstream/default/object_class_description.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::common::Plugin* plugin);
    virtual ~Engine() override;

    virtual nx::sdk::analytics::common::Plugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    virtual void executeAction(nx::sdk::analytics::Action*, nx::sdk::Error*) override;

    std::vector<ObjectClassDescription> objectClassDescritions() const;

    std::chrono::microseconds currentTimeUs() const;

    nx::sdk::Error setHandler(nx::sdk::analytics::IEngine::IHandler* handler);
    
    virtual bool isCompatible(const nx::sdk::DeviceInfo* deviceInfo) const override;

private:
    std::vector<ObjectClassDescription> loadObjectClasses() const;
    std::vector<std::string> loadLabels(const std::string& labelFilePath) const;
    std::vector<std::string> loadClassGuids(const std::string& guidsFilePath) const;

    std::string buildManifestObectTypeString(const ObjectClassDescription& description) const;

private:
    nx::sdk::analytics::common::Plugin* const m_plugin;
    mutable std::vector<ObjectClassDescription> m_objectClassDescritions;
    mutable std::string m_manifest;
    std::unique_ptr<
        nxpl::TimeProvider,
        std::function<void(nxpl::TimeProvider*)>> m_timeProvider;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

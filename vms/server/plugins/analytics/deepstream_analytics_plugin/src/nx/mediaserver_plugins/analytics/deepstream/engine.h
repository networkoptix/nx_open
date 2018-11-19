#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/sdk/analytics/common_plugin.h>

#include <nx/mediaserver_plugins/analytics/deepstream/default/object_class_description.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace deepstream {

class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
{
public:
    Engine(nx::sdk::analytics::CommonPlugin* plugin);
    virtual ~Engine() override;

    virtual nx::sdk::analytics::CommonPlugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::Settings* settings) override;

    virtual nx::sdk::Settings* settings() const override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    virtual void executeAction(nx::sdk::analytics::Action*, nx::sdk::Error*) override;

    std::vector<ObjectClassDescription> objectClassDescritions() const;

    std::chrono::microseconds currentTimeUs() const;
    
    nx::sdk::Error setHandler(nx::sdk::analytics::Engine::IHandler* handler);

private:
    std::vector<ObjectClassDescription> loadObjectClasses() const;
    std::vector<std::string> loadLabels(const std::string& labelFilePath) const;
    std::vector<std::string> loadClassGuids(const std::string& guidsFilePath) const;

    std::string buildManifestObectTypeString(const ObjectClassDescription& description) const;

private:
    nx::sdk::analytics::CommonPlugin* const m_plugin;
    mutable std::vector<ObjectClassDescription> m_objectClassDescritions;
    mutable std::string m_manifest;
    std::unique_ptr<
        nxpl::TimeProvider,
        std::function<void(nxpl::TimeProvider*)>> m_timeProvider;
};

} // namespace deepstream
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

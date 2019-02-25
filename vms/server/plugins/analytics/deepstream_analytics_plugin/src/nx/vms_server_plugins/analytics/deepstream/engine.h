#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>

#include <nx/sdk/helpers/ref_countable.h>
#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/i_time_utility_provider.h>

#include <nx/vms_server_plugins/analytics/deepstream/default/object_class_description.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class Engine: public nx::sdk::RefCountable<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);
    virtual ~Engine() override;

    virtual nx::sdk::analytics::Plugin* plugin() const override { return m_plugin; }
    
    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    virtual void executeAction(nx::sdk::analytics::IAction*, nx::sdk::Error*) override;

    std::vector<ObjectClassDescription> objectClassDescritions() const;

    std::chrono::microseconds currentTimeUs() const;

    nx::sdk::Error setHandler(nx::sdk::analytics::IEngine::IHandler* handler);

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

private:
    std::vector<ObjectClassDescription> loadObjectClasses() const;
    std::vector<std::string> loadLabels(const std::string& labelFilePath) const;
    std::vector<std::string> loadClassGuids(const std::string& guidsFilePath) const;

    std::string buildManifestObectTypeString(const ObjectClassDescription& description) const;

private:
    nx::sdk::analytics::Plugin* const m_plugin;
    mutable std::vector<ObjectClassDescription> m_objectClassDescritions;
    mutable std::string m_manifest;
    nx::sdk::Ptr<nx::sdk::ITimeUtilityProvider> m_timeUtilityProvider;
    nx::sdk::analytics::IDeviceAgent* m_deviceAgent = nullptr;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

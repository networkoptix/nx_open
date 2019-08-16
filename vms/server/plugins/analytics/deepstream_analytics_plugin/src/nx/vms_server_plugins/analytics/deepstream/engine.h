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

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    std::vector<ObjectClassDescription> objectClassDescritions() const;

    std::chrono::microseconds currentTimeUs() const;

    void setHandler(nx::sdk::analytics::IEngine::IHandler* handler);

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::IStringMap*>* outResult,
        const nx::sdk::IStringMap* settings) override;
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;
    virtual void doExecuteAction(
        nx::sdk::Result<nx::sdk::analytics::IAction::Result>* outResult,
        const nx::sdk::analytics::IAction* action) override;

private:
    std::vector<ObjectClassDescription> loadObjectClasses() const;
    std::vector<std::string> loadLabels(const std::string& labelFilePath) const;
    std::vector<std::string> loadClassGuids(const std::string& guidsFilePath) const;

    std::string buildManifestObectTypeString(const ObjectClassDescription& description) const;

private:
    nx::sdk::analytics::Plugin* const m_plugin;
    
    mutable std::vector<ObjectClassDescription> m_objectClassDescritions;
    mutable std::string m_manifest;
    nx::sdk::analytics::IDeviceAgent* m_deviceAgent = nullptr;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

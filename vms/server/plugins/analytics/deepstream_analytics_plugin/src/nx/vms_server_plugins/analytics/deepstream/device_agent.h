#pragma once

#include <thread>
#include <memory>
#include <mutex>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/ref_countable.h>

#include <nx/vms_server_plugins/analytics/deepstream/engine.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/vms_server_plugins/analytics/deepstream/pipeline_builder.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class DeviceAgent: public nx::sdk::RefCountable<nx::sdk::analytics::IConsumingDeviceAgent>
{
public:
    DeviceAgent(Engine* engine, const std::string& id);

    virtual ~DeviceAgent() override;

    virtual nx::sdk::StringMapResult setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::SettingsResponseResult pluginSideSettings() const override;

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual nx::sdk::Result<void> setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual nx::sdk::StringResult manifest() const override;

    virtual nx::sdk::Result<void> pushDataPacket(
        nx::sdk::analytics::IDataPacket* dataPacket) override;

private:
    void startFetchingMetadata(const nx::sdk::analytics::IMetadataTypes* metadataTypes);
    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;
    std::unique_ptr<nx::gstreamer::Pipeline> m_pipeline;
    mutable std::mutex m_mutex;
    mutable std::string m_manifest;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

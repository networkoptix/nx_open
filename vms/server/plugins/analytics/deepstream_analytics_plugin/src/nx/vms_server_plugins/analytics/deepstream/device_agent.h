#pragma once

#include <thread>
#include <memory>
#include <mutex>

#include <plugins/plugin_tools.h>

#include <nx/vms_server_plugins/analytics/deepstream/engine.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/vms_server_plugins/analytics/deepstream/pipeline_builder.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class DeviceAgent: public nxpt::CommonRefCounter<nx::sdk::analytics::IConsumingDeviceAgent>
{
public:
    DeviceAgent(Engine* engine, const std::string& id);

    virtual ~DeviceAgent() override;

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    virtual nx::sdk::Error setHandler(nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual nx::sdk::Error pushDataPacket(nx::sdk::analytics::IDataPacket* dataPacket) override;

private:
    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    nx::sdk::analytics::IDeviceAgent::IHandler* m_handler;
    std::unique_ptr<nx::gstreamer::Pipeline> m_pipeline;
    mutable std::mutex m_mutex;
    mutable std::string m_manifest;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

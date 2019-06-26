#pragma once

#include <thread>
#include <memory>
#include <mutex>

#include <nx/sdk/helpers/ptr.h>
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

    virtual Engine* engine() const override { return m_engine; }

    virtual void setSettings(
        const nx::sdk::IStringMap* settings,
        nx::sdk::IError* outError) override;

    virtual nx::sdk::IStringMap* pluginSideSettings(nx::sdk::IError* outError) const override;

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual void setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes,
        nx::sdk::IError* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::IError* error) const override;

    virtual void pushDataPacket(
        nx::sdk::analytics::IDataPacket* dataPacket,
        nx::sdk::IError* outError) override;

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

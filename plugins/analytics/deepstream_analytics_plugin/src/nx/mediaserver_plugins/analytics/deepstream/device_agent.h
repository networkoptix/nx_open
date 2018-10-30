#pragma once

#include <thread>
#include <memory>
#include <mutex>

#include <plugins/plugin_tools.h>

#include <nx/mediaserver_plugins/analytics/deepstream/engine.h>
#include <nx/sdk/analytics/consuming_device_agent.h>
#include <nx/mediaserver_plugins/analytics/deepstream/pipeline_builder.h>

#include "engine.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace deepstream {

class DeviceAgent: public nxpt::CommonRefCounter<nx::sdk::analytics::ConsumingDeviceAgent>
{
public:
    DeviceAgent(Engine* engine, const std::string& id);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::Settings* settings) override;

    virtual nx::sdk::Settings* settings() const override;

    virtual nx::sdk::Error setMetadataHandler(
        nx::sdk::analytics::MetadataHandler* metadataHandler) override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const char* manifest(
        nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    virtual nx::sdk::Error pushDataPacket(nx::sdk::analytics::DataPacket* dataPacket) override;

private:
    nx::sdk::Error startFetchingMetadata(
        const char* const* typeList, int typeListSize);
   
    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    nx::sdk::analytics::MetadataHandler* m_metadataHandler;
    std::unique_ptr<nx::gstreamer::Pipeline> m_pipeline;
    mutable std::mutex m_mutex;
    mutable std::string m_manifest;
};

} // namespace stub
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

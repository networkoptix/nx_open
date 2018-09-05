#pragma once

#include <thread>
#include <memory>
#include <mutex>

#include <plugins/plugin_tools.h>

#include <nx/mediaserver_plugins/metadata/deepstream/plugin.h>
#include <nx/sdk/metadata/consuming_camera_manager.h>
#include <nx/mediaserver_plugins/metadata/deepstream/pipeline_builder.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::ConsumingCameraManager>
{
public:
    Manager(
        nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin,
        const std::string& id);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

    virtual nx::sdk::Error startFetchingMetadata(
        const char* const* typeList, int typeListSize) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::metadata::MetadataHandler* handler) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    virtual nx::sdk::Error pushDataPacket(nx::sdk::metadata::DataPacket* dataPacket) override;

private:
    nx::mediaserver_plugins::metadata::deepstream::Plugin* const m_plugin;
    nx::sdk::metadata::MetadataHandler* m_handler;
    std::unique_ptr<nx::gstreamer::Pipeline> m_pipeline;
    mutable std::mutex m_mutex;
    mutable std::string m_manifest;
};

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

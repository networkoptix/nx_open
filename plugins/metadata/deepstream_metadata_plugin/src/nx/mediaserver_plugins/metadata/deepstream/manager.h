#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_consuming_metadata_manager.h>

#include "plugin.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractConsumingMetadataManager>
{
public:
    Manager(Plugin* plugin);
    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* /*eventTypeList*/,
        int /*eventTypeListSize*/) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::metadata::AbstractMetadataHandler* handler) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    virtual nx::sdk::Error putData(nx::sdk::metadata::AbstractDataPacket* dataPacket) override;

private:
    Plugin* const m_plugin;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;
    mutable std::mutex m_mutex;
};

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

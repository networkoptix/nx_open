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
namespace stub {

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractConsumingMetadataManager>
{
public:
    Manager(Plugin* plugin);
    virtual ~Manager();

    virtual Plugin* plugin() override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* /*eventTypeList*/,
        int /*eventTypeListSize*/) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::metadata::AbstractMetadataHandler* handler) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) const override;

    virtual nx::sdk::Error putData(nx::sdk::metadata::AbstractDataPacket* dataPacket) override;

private:
    nx::sdk::Error stopFetchingMetadataUnsafe();
    nx::sdk::metadata::AbstractMetadataPacket* cookSomeEvents();
    nx::sdk::metadata::AbstractMetadataPacket* cookSomeObjects(
        nx::sdk::metadata::AbstractDataPacket* mediaPacket);

    int64_t usSinceEpoch() const;

private:
    Plugin* const m_plugin;
    mutable std::mutex m_mutex;
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stopping{false};
    nx::sdk::metadata::AbstractMetadataHandler* m_handler = nullptr;
    int m_counter = 0;
    int m_counterObjects = 0;
    nxpl::NX_GUID m_eventTypeId;
};

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

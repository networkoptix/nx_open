#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>


namespace nx {
namespace mediaserver {
namespace plugins {

class StubMetadataManager:
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataManager>
{
public:
    StubMetadataManager();

    virtual ~StubMetadataManager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::AbstractMetadataHandler* handler,
        nxpl::NX_GUID* /*eventTypeList*/,
        int /*eventTypeListSize*/) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) const override;

private:
    nx::sdk::Error stopFetchingMetadataUnsafe();
    nx::sdk::metadata::AbstractMetadataPacket* cookSomeEvents();

    int64_t usSinceEpoch() const;

private:
    mutable std::mutex m_mutex;
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stopping;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;
    int m_counter = 0;
    nxpl::NX_GUID m_eventTypeId;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>


namespace nx {
namespace sdk {
namespace metadata {

class StubMetadataManager: public nxpt::CommonRefCounter<AbstractMetadataManager>
{
public:
    StubMetadataManager();

    virtual ~StubMetadataManager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual Error startFetchingMetadata(AbstractMetadataHandler* handler) override;

    virtual Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(Error* error) const override;

private:
    Error stopFetchingMetadataUnsafe();
    AbstractMetadataPacket* cookSomeEvents();

    int64_t usSinceEpoch() const;

private:
    mutable std::mutex m_mutex;
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stopping;
    AbstractMetadataHandler* m_handler;
    int m_counter = 0;
    nxpl::NX_GUID m_eventTypeId;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
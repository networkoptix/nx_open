#pragma once

#include <core/resource/abstract_remote_archive_manager.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaResource;

class HanwhaRemoteArchiveManager: public
    nx::core::resource::AbstractRemoteArchiveManager
{
    using EntriesUpdatedCallback =
        std::function<void(const std::vector<nx::core::resource::RemoteArchiveChunk>&)>;
public:
    HanwhaRemoteArchiveManager(HanwhaResource* resource);
    virtual ~HanwhaRemoteArchiveManager() = default;

    virtual bool listAvailableArchiveEntries(
        nx::core::resource::OverlappedRemoteChunks* outArchiveEntries,
        int64_t startTimeMs = 0,
        int64_t endTimeMs = std::numeric_limits<int64_t>::max()) override;

    virtual bool fetchArchiveEntry(
        const QString& entryId,
        nx::core::resource::BufferType* outBuffer) override;

    virtual std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate(
        const nx::core::resource::RemoteArchiveChunk& chunk) override;

    virtual bool removeArchiveEntries(const std::vector<QString>& entryIds) override;

    virtual nx::core::resource::RemoteArchiveCapabilities capabilities() const override;

    virtual nx::core::resource::RemoteArchiveSynchronizationSettings settings() const;

    virtual void beforeSynchronization() override;

    virtual void afterSynchronization(bool isSynchronizationSuccessful) override;

private:
    HanwhaResource* m_resource;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

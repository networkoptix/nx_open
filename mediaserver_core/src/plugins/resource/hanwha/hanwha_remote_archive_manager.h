#pragma once

#if defined(ENABLE_HANWHA)

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
    virtual ~HanwhaRemoteArchiveManager();

    // Implemantation of AbstractRemoteArchiveManager::listAvailableArchiveEntries
    virtual bool listAvailableArchiveEntries(
        std::vector<nx::core::resource::RemoteArchiveChunk>* outArchiveEntries,
        int64_t startTimeMs = 0,
        int64_t endTimeMs = std::numeric_limits<int64_t>::max()) override;

    // Implemantation of AbstractRemoteArchiveManager::setOnAvailableEntriesCallback
    virtual void setOnAvailabaleEntriesUpdatedCallback(EntriesUpdatedCallback callback) override;

    // Implemantation of AbstractRemoteArchiveManager::fetchArchiveEntry
    virtual bool fetchArchiveEntry(
        const QString& entryId,
        nx::core::resource::BufferType* outBuffer) override;

    // Implemantation of AbstractRemoteArchiveManager::archiveDelegate
    virtual QnAbstractArchiveDelegate* archiveDelegate() override;

    // Implemantation of AbstractRemoteArchiveManager::removeArchiveEntries
    virtual bool removeArchiveEntries(const std::vector<QString>& entryIds) override;

    // Implemantation of AbstractRemoteArchiveManager::capabilities
    virtual nx::core::resource::RemoteArchiveCapabilities capabilities() const override;

private:
    HanwhaResource* m_resource;
    EntriesUpdatedCallback m_callback;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)

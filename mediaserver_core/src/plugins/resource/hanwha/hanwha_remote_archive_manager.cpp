#if defined(ENABLE_HANWHA)

#include "hanwha_resource.h"
#include "hanwha_remote_archive_manager.h"
#include "hanwha_archive_delegate.h"
#include "hanwha_chunk_reader.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

using namespace nx::core::resource;

HanwhaRemoteArchiveManager::HanwhaRemoteArchiveManager(HanwhaResource* resource):
    m_resource(resource)
{
}

bool HanwhaRemoteArchiveManager::listAvailableArchiveEntries(
    std::vector<RemoteArchiveChunk>* outArchiveEntries,
    int64_t /*startTimeMs*/,
    int64_t /*endTimeMs*/)
{
    // TODO: #dmishin Fix channel if needed
    const auto chunks = m_resource->sharedContext()->chunksSync(m_resource->getChannel());

    for (const auto& chunk: chunks)
    {
        outArchiveEntries->emplace_back(
            lit("%1-%2").arg(chunk.startTimeMs).arg(chunk.endTimeMs()),
            chunk.startTimeMs,
            chunk.durationMs);
    }

    return true;
}

bool HanwhaRemoteArchiveManager::fetchArchiveEntry(
    const QString& /*entryId*/,
    nx::core::resource::BufferType* /*outBuffer*/)
{
    return false;
}

bool HanwhaRemoteArchiveManager::removeArchiveEntries(const std::vector<QString>& /*entryIds*/)
{
    return false;
}

RemoteArchiveCapabilities HanwhaRemoteArchiveManager::capabilities() const
{
    return RemoteArchiveCapability::RandomAccessChunkCapability;
}

std::unique_ptr<QnAbstractArchiveDelegate> HanwhaRemoteArchiveManager::archiveDelegate()
{
    return m_resource->remoteArchiveDelegate();
}

void HanwhaRemoteArchiveManager::setOnAvailabaleEntriesUpdatedCallback(
    EntriesUpdatedCallback callback)
{
    m_callback = callback;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)

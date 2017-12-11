#if defined(ENABLE_HANWHA)

#include "hanwha_resource.h"
#include "hanwha_remote_archive_manager.h"
#include "hanwha_archive_delegate.h"
#include "hanwha_chunk_reader.h"

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const std::chrono::milliseconds kWaitBeforeSync(20000);
static const std::chrono::milliseconds kWaitBeforeNextChunk(3000);
static const int kNumberOfSyncCycles = 2;
static const int kTriesPerChunk = 3;

} // namespace

using namespace nx::core::resource;

HanwhaRemoteArchiveManager::HanwhaRemoteArchiveManager(HanwhaResource* resource):
    m_resource(resource)
{
}

bool HanwhaRemoteArchiveManager::listAvailableArchiveEntries(
    OverlappedRemoteChunks* outArchiveEntries,
    int64_t /*startTimeMs*/,
    int64_t /*endTimeMs*/)
{
    // TODO: #dmishin Fix channel if needed
    const auto overlappedPeriods = m_resource
        ->sharedContext()
        ->overlappedTimelineSync(m_resource->getChannel());

    for (const auto& entry: overlappedPeriods)
    {
        const auto overlappedId = entry.first;
        const auto& periods = entry.second;

        for (const auto& period: periods)
        {
            (*outArchiveEntries)[overlappedId].emplace_back(
                QString(),
                period.startTimeMs,
                period.durationMs,
                overlappedId);
        }
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

std::unique_ptr<QnAbstractArchiveDelegate> HanwhaRemoteArchiveManager::archiveDelegate(
    const RemoteArchiveChunk& chunk)
{
    auto delegate = m_resource->remoteArchiveDelegate();
    auto hanwhaDelegate = dynamic_cast<HanwhaArchiveDelegate*>(delegate.get());

    if (hanwhaDelegate)
        hanwhaDelegate->setOverlappedId(chunk.overlappedId);

    return delegate;
}

void HanwhaRemoteArchiveManager::beforeSynchronization()
{
    // Do nothing.
}

void HanwhaRemoteArchiveManager::afterSynchronization(bool isSynchronizationSuccessful)
{
    if (!isSynchronizationSuccessful)
    {
        NX_INFO(
            this,
            lm("Synchronization for resource %1 was not successful. "
                "Date and time synchronization will not be done.")
                .arg(m_resource->getUserDefinedName()));
        return;
    }

    const auto dateTime = qnSyncTime->currentDateTime();
    NX_INFO(this, lm("Setting date and time (%1) for resource %2")
        .args(dateTime, m_resource->getUserDefinedName()));
    m_resource->sharedContext()->setDateTime(dateTime);
}

RemoteArchiveSynchronizationSettings HanwhaRemoteArchiveManager::settings() const
{
    return {
        kWaitBeforeSync,
        kWaitBeforeNextChunk,
        kNumberOfSyncCycles,
        kTriesPerChunk
    };
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)

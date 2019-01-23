#include "hanwha_resource.h"
#include "hanwha_remote_archive_manager.h"
#include "hanwha_archive_delegate.h"
#include "hanwha_chunk_loader.h"
#include "hanwha_time_synchronizer.h"

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

namespace nx {
namespace vms::server {
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
    const auto context = m_resource->sharedContext();

    std::optional<std::chrono::milliseconds> timeShift;
    for (int i = 0; i < kTriesPerChunk && !timeShift; ++i)
        timeShift = hanwhaUtcTimeShift(context);

    // TODO: Change assert into something else if it happens.
    if (NX_ASSERT(timeShift, "Unable to fetch timeshift before chunk loading"))
        context->setTimeShift(*timeShift);

    // TODO: #dmishin Fix channel if needed.
    const auto overlappedPeriods = context->overlappedTimelineSync(m_resource->getChannel());
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

nx::core::resource::ImportOrder HanwhaRemoteArchiveManager::overlappedIdImportOrder() const
{
    return nx::core::resource::ImportOrder::Reverse;
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

void HanwhaRemoteArchiveManager::afterSynchronization(bool /*isSynchronizationSuccessful*/)
{
    // Do nothing.
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
} // namespace vms::server
} // namespace nx

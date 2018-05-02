#include "base_remote_archive_synchronization_task.h"
#include "remote_archive_common.h"

#include <motion/motion_helper.h>
#include <recorder/storage_manager.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#include <nx/mediaserver/event/event_connector.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const std::chrono::milliseconds kDetalizationLevel(1);
static const std::chrono::milliseconds kMinChunkDuration(1000);
static const std::chrono::milliseconds kStartTimestampThreshold(10000);
static const QString kRecorderThreadName = lit("Edge recorder");
static const int64_t kMaxRecordingDiffMs = 2000;

} // namespace

using namespace std::chrono;
using namespace nx::core::resource;

BaseRemoteArchiveSynchronizationTask::BaseRemoteArchiveSynchronizationTask(
    QnMediaServerModule* serverModule,
    const QnSecurityCamResourcePtr& resource)
    :
    AbstractRemoteArchiveSynchronizationTask(serverModule),
    m_resource(resource)
{
}

QnUuid BaseRemoteArchiveSynchronizationTask::id() const
{
    return m_resource->getId();
}

void BaseRemoteArchiveSynchronizationTask::setDoneHandler(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_doneHandler = std::move(handler);
}

void BaseRemoteArchiveSynchronizationTask::cancel()
{
    QnMutexLocker lock(&m_mutex);

    NX_VERBOSE(this, lm("Resource: %1. Synchronization task has been canceled")
        .arg(m_resource->getUserDefinedName()));

    m_canceled = true;

    if (m_archiveReader)
        m_archiveReader->pleaseStop();

    if (m_recorder)
        m_recorder->pleaseStop();

    m_wait.wakeAll();
}

bool BaseRemoteArchiveSynchronizationTask::execute()
{
    auto archiveManager = m_resource->remoteArchiveManager();
    NX_ASSERT(archiveManager);
    if (!archiveManager)
        return false;

    m_settings = archiveManager->settings();

    archiveManager->beforeSynchronization();
    qnEventRuleConnector->at_remoteArchiveSyncStarted(m_resource);
    bool result = true;
    for (auto i = 0; i < m_settings.syncCyclesNumber; ++i)
    {
        if(m_settings.waitBeforeSync > milliseconds::zero())
        {
            QnMutexLocker lock(&m_mutex);
            m_wait.wait(
                &m_mutex,
                duration_cast<milliseconds>(m_settings.waitBeforeSync).count());
        }

        if (m_canceled)
        {
            result = false;
            break;
        }

        result &= synchronizeArchive();
    }

    archiveManager->afterSynchronization(result);
    qnEventRuleConnector->at_remoteArchiveSyncFinished(m_resource);
    return result;
}

bool BaseRemoteArchiveSynchronizationTask::synchronizeArchive()
{
    NX_INFO(
        this,
        lm("Starting archive synchronization. Resource: %1 %2")
            .args(m_resource->getUserDefinedName(), m_resource->getUniqueId()));

    if (!fetchChunks(&m_chunks))
        return false;

    if (m_chunks.empty())
        return true;

    m_totalDuration += calculateDurationOfMediaToImport();

    if (m_totalDuration == std::chrono::milliseconds::zero())
        return true;

    bool result = true;
    for (const auto& entry: m_chunks)
    {
        const auto overlappedId = entry.first;
        const auto& chunks = entry.second;

        if (!chunks.empty())
            result &= synchronizeOverlappedTimeline(overlappedId);
    }

    return result;
}

bool BaseRemoteArchiveSynchronizationTask::synchronizeOverlappedTimeline(
    const OverlappedId& overlappedId)
{
    NX_DEBUG(this, lm("Synchronizing overlapped ID %1. Device: %2.")
        .args(overlappedId, m_resource->getUserDefinedName()));

    NX_ASSERT(m_chunks.find(overlappedId) != m_chunks.cend());
    const auto startTimeMs = m_chunks[overlappedId].front().startTimeMs;
    const auto endTimeMs = m_chunks[overlappedId].back().startTimeMs
        + m_chunks[overlappedId].back().durationMs;

    NX_CRITICAL(qnNormalStorageMan);
    auto serverTimePeriods = qnNormalStorageMan
        ->getFileCatalog(m_resource->getUniqueId(), QnServer::ChunksCatalog::HiQualityCatalog)
        ->getTimePeriods(
            startTimeMs,
            endTimeMs,
            duration_cast<milliseconds>(kDetalizationLevel).count(),
            /*keepSmallChunks*/ true,
            std::numeric_limits<int>::max());

    auto deviceTimePeriods = toTimePeriodList(m_chunks[overlappedId]);
    NX_DEBUG(this, lm("Synchronizing overlapped ID %1. Device time periods: %2. Device: %3.")
        .args(overlappedId, deviceTimePeriods, m_resource->getUserDefinedName()));
    NX_DEBUG(this, lm("Synchronizing overlapped ID %1. Server time periods: %2. Device: %3.")
        .args(overlappedId, serverTimePeriods, m_resource->getUserDefinedName()));

    deviceTimePeriods.excludeTimePeriods(serverTimePeriods);
    NX_DEBUG(this, lm("Synchronizing overlapped ID %1. Periods to import from device: %2. Device %3.")
        .args(overlappedId, deviceTimePeriods, m_resource->getUserDefinedName()));

    if (deviceTimePeriods.isEmpty())
        return true;

    return writeAllTimePeriods(deviceTimePeriods, overlappedId);
}

void BaseRemoteArchiveSynchronizationTask::createStreamRecorderThreadUnsafe(
    const QnTimePeriod& timePeriod)
{
    NX_ASSERT(m_archiveReader, lit("Archive reader should be created before stream recorder"));
    m_recorder = std::make_unique<QnServerEdgeStreamRecorder>(
        m_resource,
        QnServer::ChunksCatalog::HiQualityCatalog,
        m_archiveReader.get());

    m_recorder->setObjectName(kRecorderThreadName);
    m_recorder->setRecordingBounds(
        duration_cast<microseconds>(milliseconds(timePeriod.startTimeMs)),
        duration_cast<microseconds>(milliseconds(timePeriod.endTimeMs())));
    m_recorder->setStartTimestampThreshold(kStartTimestampThreshold);

    m_recorder->setSaveMotionHandler(
        [this](const QnConstMetaDataV1Ptr& motion){ return saveMotion(motion); });

    m_recorder->setOnFileWrittenHandler(
        [this](
            milliseconds /*startTime*/,
            milliseconds duration)
        {
            onFileHasBeenWritten(duration);
        });

    m_recorder->setEndOfRecordingHandler([this](){ onEndOfRecording(); });
}

bool BaseRemoteArchiveSynchronizationTask::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (motion)
    {
        NX_VERBOSE(this, lm("Saving motion data packet with timestamp %1. Device: %2.")
            .args(microseconds(motion->timestamp), m_resource->getUserDefinedName()));

        auto helper = QnMotionHelper::instance();
        QnMotionArchive* archive = helper->getArchive(
            m_resource,
            motion->channelNumber);

        if (archive)
            archive->saveToArchive(motion);
    }

    return true;
}

bool BaseRemoteArchiveSynchronizationTask::fetchChunks(
    OverlappedRemoteChunks* outChunks)
{
    NX_ASSERT(outChunks);
    if (!outChunks)
        return false;

    auto manager = m_resource->remoteArchiveManager();
    if (!manager)
    {
        NX_ERROR(
            this,
            lm("Resource %1 %2 has no remote archive manager")
            .args(m_resource->getUserDefinedName(), m_resource->getUniqueId()));

        return false;
    }

    m_chunks.clear();
    auto result = manager->listAvailableArchiveEntries(outChunks);
    if (!result)
    {
        NX_ERROR(
            this,
            lm("Can not fetch chunks from camera %1 %2")
            .args(m_resource->getUserDefinedName(), m_resource->getUniqueId()));

        return false;
    }

    if (outChunks->empty())
    {
        NX_DEBUG(
            this,
            lm("Device chunks list is empty for resource %1")
            .arg(m_resource->getUserDefinedName()));
    }

    return true;
}

bool BaseRemoteArchiveSynchronizationTask::writeAllTimePeriods(
    const QnTimePeriodList& timePeriods,
    OverlappedId overlappedId)
{
    auto manager = m_resource->remoteArchiveManager();
    NX_ASSERT(manager);
    if (!manager)
        return false;

    const auto settings = manager->settings();
    for (const auto& timePeriod : timePeriods)
    {
        if (m_canceled)
            break;

        if (timePeriod.durationMs >= duration_cast<milliseconds>(kMinChunkDuration).count())
        {
            if (settings.waitBetweenChunks > milliseconds::zero())
            {
                QnMutexLocker lock(&m_mutex);
                m_wait.wait(
                    &m_mutex,
                    duration_cast<milliseconds>(settings.waitBetweenChunks).count());

                if (m_canceled)
                    break;
            }

            const auto chunk = remoteArchiveChunkByTimePeriod(timePeriod, overlappedId);
            if (chunk == boost::none)
                continue;

            writeTimePeriodToArchive(timePeriod, *chunk);
        }
    }

    return true;
}

bool BaseRemoteArchiveSynchronizationTask::writeTimePeriodToArchive(
    const QnTimePeriod& timePeriod,
    const RemoteArchiveChunk& chunk)
{
    if (m_canceled)
        return false;

    if (!prepareDataSource(timePeriod, chunk))
        return false;

    NX_DEBUG(
        this,
        lm("Writing time period %1 (%2) - %3 (%4). "
            "Chunk bounds: %5 (%6) - %7 (%8). "
            "Resource %9")
            .args(
                QDateTime::fromMSecsSinceEpoch(timePeriod.startTimeMs),
                timePeriod.startTimeMs,
                QDateTime::fromMSecsSinceEpoch(timePeriod.endTimeMs()),
                timePeriod.endTimeMs(),
                QDateTime::fromMSecsSinceEpoch(chunk.startTimeMs),
                chunk.startTimeMs,
                QDateTime::fromMSecsSinceEpoch(chunk.startTimeMs + chunk.durationMs),
                chunk.startTimeMs + chunk.durationMs,
                m_resource->getUserDefinedName()));

    {
        QnMutexLocker lock(&m_mutex);
        createArchiveReaderThreadUnsafe(timePeriod, chunk);
        createStreamRecorderThreadUnsafe(timePeriod);
    }


    NX_ASSERT(
        m_archiveReader && m_recorder,
        lm("Can not create archive reader and/or recorder. Resource %1")
            .arg(m_resource->getUserDefinedName()));

    if (!m_archiveReader || !m_recorder)
        return false;

    m_archiveReader->setCycleMode(false);
    m_archiveReader->addDataProcessor(m_recorder.get());

    m_recorder->start();
    m_archiveReader->start();

    m_recorder->wait();
    m_archiveReader->wait();

    NX_DEBUG(
        this,
        lm("Time period %1 (%2) - %3 (%4) has been recorded."
            " Corresponding chunk: %5 (%6) - %7 (%8)."
            " Resource: %9")
            .args(
                QDateTime::fromMSecsSinceEpoch(timePeriod.startTimeMs),
                timePeriod.startTimeMs,
                QDateTime::fromMSecsSinceEpoch(timePeriod.endTimeMs()),
                timePeriod.endTimeMs(),
                QDateTime::fromMSecsSinceEpoch(chunk.startTimeMs),
                chunk.startTimeMs,
                QDateTime::fromMSecsSinceEpoch(chunk.startTimeMs + chunk.durationMs),
                chunk.startTimeMs + chunk.durationMs,
                m_resource->getUserDefinedName()));

    return true;
}

void BaseRemoteArchiveSynchronizationTask::onFileHasBeenWritten(
    const std::chrono::milliseconds& duration)
{
    m_importedDuration += duration;
    NX_VERBOSE(
        this,
        lm("Resource %1. File has been written. Duration: %1,"
            "Current duration of imported remote archive: %2,"
            "Total duration to import: %3")
            .args(
                m_resource->getUserDefinedName(),
                duration,
                m_importedDuration,
                m_totalDuration));

    if (!needToReportProgress())
        return;

    NX_ASSERT(m_totalDuration != std::chrono::milliseconds::zero());
    if (m_totalDuration == std::chrono::milliseconds::zero())
        return;

    auto progress =
        (double)duration_cast<milliseconds>(m_importedDuration).count()
        / duration_cast<milliseconds>(m_totalDuration).count();

    NX_ASSERT(
        progress >= 0 && progress <= 1.0,
        lm("Wrong progress! Imported: %1, Total: %2")
            .args(m_importedDuration, m_totalDuration));

    if (progress > 0.99)
        progress = 0.99;

    if (progress < 0)
        return;

    if (progress < m_progress)
        progress = m_progress;

    m_progress = progress;

    NX_CRITICAL(qnEventRuleConnector);
    qnEventRuleConnector->at_remoteArchiveSyncProgress(
        m_resource,
        progress);
}

void BaseRemoteArchiveSynchronizationTask::onEndOfRecording()
{
    NX_DEBUG(this, lit("Stopping recording from recorder. Got out of bounds packet."));
    if (m_archiveReader)
        m_archiveReader->pleaseStop();

    m_recorder->pleaseStop();
}

milliseconds BaseRemoteArchiveSynchronizationTask::calculateDurationOfMediaToImport() const
{
    auto mergedDeviceTimePeriods = mergeOverlappedChunks(m_chunks);
    const auto startTimeMs = mergedDeviceTimePeriods.front().startTimeMs;
    const auto endTimeMs = mergedDeviceTimePeriods.back().endTimeMs();

    NX_CRITICAL(qnNormalStorageMan);
    auto serverTimePeriods = qnNormalStorageMan
        ->getFileCatalog(m_resource->getUniqueId(), QnServer::ChunksCatalog::HiQualityCatalog)
        ->getTimePeriods(
            startTimeMs,
            endTimeMs,
            duration_cast<milliseconds>(kDetalizationLevel).count(),
            /*keepSmallChunks*/ true,
            std::numeric_limits<int>::max());

    mergedDeviceTimePeriods.excludeTimePeriods(serverTimePeriods);

    return totalDuration(mergedDeviceTimePeriods, kMinChunkDuration);
}

bool BaseRemoteArchiveSynchronizationTask::needToReportProgress() const
{
    return true; //< Report progress for each written file.
}

bool BaseRemoteArchiveSynchronizationTask::prepareDataSource(
    const QnTimePeriod& /*timePeriod*/,
    const nx::core::resource::RemoteArchiveChunk& /*chunk*/)
{
    return true; //< Do nothing by default.
}

boost::optional<RemoteArchiveChunk>
BaseRemoteArchiveSynchronizationTask::remoteArchiveChunkByTimePeriod(
    const QnTimePeriod& timePeriod,
    OverlappedId overlappedId) const
{
    NX_ASSERT(m_chunks.find(overlappedId) != m_chunks.cend());
    auto chunkItr = std::lower_bound(
        m_chunks.at(overlappedId).cbegin(),
        m_chunks.at(overlappedId).cend(),
        timePeriod,
        [](const RemoteArchiveChunk& chunk, const QnTimePeriod& period)
        {
            return chunk.endTimeMs() < period.endTimeMs();
        });

    if (chunkItr == m_chunks.at(overlappedId).cend())
    {
        NX_ASSERT(
            false,
            lm("No chunk for time period %1 (%2) - %3 (%4). Resource %5")
                .args(
                    QDateTime::fromMSecsSinceEpoch(timePeriod.startTimeMs),
                    timePeriod.startTimeMs,
                    QDateTime::fromMSecsSinceEpoch(timePeriod.endTimeMs()),
                    timePeriod.endTimeMs(),
                    m_resource->getUserDefinedName()));

        return boost::none;
    }

    return *chunkItr;
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

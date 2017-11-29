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
static const std::chrono::milliseconds kWaitBeforeSynchronize(30000);
static const std::chrono::milliseconds kWaitBeforeLoadNextChunk(3000);

static const int kNumberOfSynchronizationCycles = 2;
static const QString kRecorderThreadName = lit("Edge recorder");

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

    archiveManager->beforeSynchronization();
    qnEventRuleConnector->at_remoteArchiveSyncStarted(m_resource);
    bool result = true;
    for (auto i = 0; i < kNumberOfSynchronizationCycles; ++i)
    {
        {
            QnMutexLocker lock(&m_mutex);
            m_wait.wait(&m_mutex, duration_cast<milliseconds>(kWaitBeforeSynchronize).count());
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

QnUuid BaseRemoteArchiveSynchronizationTask::id() const
{
    return m_resource->getId();
}

bool BaseRemoteArchiveSynchronizationTask::synchronizeArchive()
{
    NX_INFO(
        this,
        lm("Starting archive synchronization. Resource: %1 %2")
        .args(m_resource->getUserDefinedName(), m_resource->getUniqueId()));

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
    auto result = manager->listAvailableArchiveEntries(&m_chunks);
    if (!result)
    {
        NX_ERROR(
            this,
            lm("Can not fetch chunks from camera %1 %2")
            .args(m_resource->getUserDefinedName(), m_resource->getUniqueId()));

        return false;
    }

    if (m_chunks.empty())
    {
        NX_DEBUG(
            this,
            lm("Device chunks list is empty for resource %1")
            .arg(m_resource->getUserDefinedName()));

        return true;
    }

    const auto lastDeviceChunkIndex = m_chunks.size() - 1;
    const auto startTimeMs = m_chunks[0].startTimeMs;
    const auto endTimeMs = m_chunks[lastDeviceChunkIndex].startTimeMs
        + m_chunks[lastDeviceChunkIndex].durationMs;

    auto serverTimePeriods = qnNormalStorageMan
        ->getFileCatalog(m_resource->getUniqueId(), QnServer::ChunksCatalog::HiQualityCatalog)
        ->getTimePeriods(
            startTimeMs,
            endTimeMs,
            duration_cast<milliseconds>(kDetalizationLevel).count(),
            false,
            std::numeric_limits<int>::max());

    auto deviceTimePeriods = toTimePeriodList(m_chunks);
    NX_DEBUG(this, lm("Device time periods: %1").arg(deviceTimePeriods));
    NX_DEBUG(this, lm("Server time periods: %1").arg(serverTimePeriods));

    deviceTimePeriods.excludeTimePeriods(serverTimePeriods);
    NX_DEBUG(this, lm("Periods to import from device: %1").arg(deviceTimePeriods));

    if (deviceTimePeriods.isEmpty())
        return true;

    m_totalDuration = totalDuration(deviceTimePeriods, kMinChunkDuration);
    NX_DEBUG(
        this,
        lm("Total remote archive length to synchronize: %1")
        .arg(m_totalDuration));

    for (const auto& timePeriod : deviceTimePeriods)
    {
        if (m_canceled)
            break;

        if (timePeriod.durationMs >= duration_cast<milliseconds>(kMinChunkDuration).count())
        {
            {
                QnMutexLocker lock(&m_mutex);
                m_wait.wait(
                    &m_mutex,
                    duration_cast<milliseconds>(kWaitBeforeLoadNextChunk).count());

                if (m_canceled)
                    break;
            }

            const auto chunk = remoteArchiveChunkByTimePeriod(timePeriod);
            if (chunk == boost::none)
                continue;

            writeTimePeriodToArchive(timePeriod, *chunk);
        }
    }

    return true;
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

    m_recorder->setSaveMotionHandler(
        [this](const QnConstMetaDataV1Ptr& motion) { return saveMotion(motion); });

    m_recorder->setOnFileWrittenHandler(
        [this](
            milliseconds /*startTime*/,
            milliseconds duration)
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

            auto progress =
                (double)duration_cast<milliseconds>(m_importedDuration).count()
                / duration_cast<milliseconds>(m_totalDuration).count();

            if (progress > 1.0)
            {
                progress = 1.0;
                NX_WARNING(
                    this,
                    lm("Wrong progress! Imported: %1, Total: %2")
                    .args(m_importedDuration, m_totalDuration));
            }

            qnEventRuleConnector->at_remoteArchiveSyncProgress(
                m_resource,
                progress);

        });

    m_recorder->setEndOfRecordingHandler(
        [this]()
        {
            NX_DEBUG(this, lit("Stopping recording from recorder. Got out of bounds packet."));
            if (m_archiveReader)
                m_archiveReader->pleaseStop();

            m_recorder->pleaseStop();
        });
}

bool BaseRemoteArchiveSynchronizationTask::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (motion)
    {
        auto helper = QnMotionHelper::instance();
        QnMotionArchive* archive = helper->getArchive(
            m_resource,
            motion->channelNumber);

        if (archive)
            archive->saveToArchive(motion);
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
        createArchiveReaderThreadUnsafe(timePeriod);
        createStreamRecorderThreadUnsafe(timePeriod);
    }

    NX_ASSERT(
        m_archiveReader && m_recorder,
        lm("Can not create archive reader and/or recorder. Resource %1")
        .arg(m_resource->getUserDefinedName()));

    if (!m_archiveReader || !m_recorder)
        return false;

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
    const QnTimePeriod& timePeriod) const
{
    auto chunkItr = std::lower_bound(
        m_chunks.cbegin(),
        m_chunks.cend(),
        timePeriod,
        [](const RemoteArchiveChunk& chunk, const QnTimePeriod& period)
        {
            return chunk.startTimeMs < period.startTimeMs;
        });

    if (chunkItr == m_chunks.cend())
    {
        NX_ERROR(
            this,
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

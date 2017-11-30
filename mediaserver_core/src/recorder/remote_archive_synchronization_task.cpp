#include "remote_archive_synchronization_task.h"
#include "remote_archive_common.h"

#include <recorder/storage_manager.h>
#include <recording/stream_recorder.h>
#include <nx/mediaserver/event/event_connector.h>
#include <transcoding/transcoding_utils.h>
#include <utils/common/util.h>
#include <motion/motion_helper.h>
#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>

#include <core/resource/avi/avi_archive_delegate.h>
#include <core/storage/memory/ext_iodevice_storage.h>
#include <plugins/utils/motion_delegate_wrapper.h>
#include <core/resource/avi/avi_resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const std::chrono::milliseconds kDetalizationLevel(1);
static const std::chrono::milliseconds kMinChunkDuration(1000);
static const int kNumberOfSynchronizationCycles = 2;
static const std::chrono::milliseconds kWaitBeforeSynchronize(20000);
static const std::chrono::milliseconds kWaitBeforeLoadNextChunk(0);

static const QString kRecorderThreadName = lit("Edge recorder");
static const QString kReaderThreadName = lit("Edge reader");

} // namespace

using namespace nx::core::resource;
using namespace std::chrono;

RemoteArchiveSynchronizationTask::RemoteArchiveSynchronizationTask(
    QnMediaServerModule* serverModule,
    const QnSecurityCamResourcePtr& resource)
    :
    AbstractRemoteArchiveSynchronizationTask(serverModule),
    m_resource(resource)
{
}

void RemoteArchiveSynchronizationTask::setDoneHandler(std::function<void()> handler)
{
    m_doneHandler = handler;
}

void RemoteArchiveSynchronizationTask::cancel()
{
    NX_DEBUG(
        this,
        lm("Remote archive synchronization task has been canceled for resource %1")
            .arg(m_resource->getUserDefinedName()));

    QnMutexLocker lock(&m_mutex);
    m_canceled = true;

    if (m_archiveReader)
        m_archiveReader->pleaseStop();

    if (m_recorder)
        m_recorder->pleaseStop();

    m_wait.wakeAll();
}

bool RemoteArchiveSynchronizationTask::execute()
{
    bool result = true;
    qnEventRuleConnector->at_remoteArchiveSyncStarted(m_resource);
    for (auto i = 0; i < kNumberOfSynchronizationCycles; ++i)
    {
         result &= synchronizeArchive();
         if (!result)
             break;
    }

    qnEventRuleConnector->at_remoteArchiveSyncFinished(m_resource);
    return result;
}

QnUuid RemoteArchiveSynchronizationTask::id() const
{
    return m_resource->getId();
}

bool RemoteArchiveSynchronizationTask::synchronizeArchive()
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
                    return false;
            }

            writeTimePeriodToArchive(timePeriod);
        }
    }

    return true;
}

bool RemoteArchiveSynchronizationTask::writeTimePeriodToArchive(const QnTimePeriod& timePeriod)
{
    if (m_canceled)
        return false;

    const auto chunk = remoteArchiveChunkByTimePeriod(timePeriod);
    if (chunk == boost::none)
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

        return false;
    }

    auto manager = m_resource->remoteArchiveManager();
    if (!manager)
    {
        NX_ERROR(
            this,
            lm("Resource %1 %2 has no remote archive manager")
                .args(m_resource->getUserDefinedName(), m_resource->getUniqueId()));

        return false;
    }

    if (chunk != m_currentChunk)
    {
        m_currentChunk = *chunk;
        m_currentChunkBuffer.clear();
        if (!manager->fetchArchiveEntry(chunk->id, &m_currentChunkBuffer))
        {
            NX_DEBUG(
                this,
                lm("Can not fetch archive chunk %1. Resource %2")
                    .args(chunk->id, m_resource->getUserDefinedName()));

            return false;
        }
        else
        {
            NX_DEBUG(
                this,
                lm("Archive chunk with id %1 has been fetched, size %2 bytes")
                    .args(chunk->id, m_currentChunkBuffer.size()));
        }
    }

    return writeTimePeriodInternal(timePeriod, chunk.get());
}

bool RemoteArchiveSynchronizationTask::writeTimePeriodInternal(
    const QnTimePeriod& timePeriod,
    const RemoteArchiveChunk& chunk)
{
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

    createArchiveReaderThreadUnsafe(timePeriod, chunk);
    createStreamRecorderThreadUnsafe(timePeriod, chunk);

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

void RemoteArchiveSynchronizationTask::createArchiveReaderThreadUnsafe(
    const QnTimePeriod& timePeriod,
    const nx::core::resource::RemoteArchiveChunk& chunk)
{
    auto ioDevice = new QBuffer(); //< Will be freed by delegate.
    ioDevice->setBuffer(&m_currentChunkBuffer);
    ioDevice->open(QIODevice::ReadOnly);

    const auto temporaryFilePath = QString::number(nx::utils::random::number());
    QnExtIODeviceStorageResourcePtr storage(new QnExtIODeviceStorageResource(commonModule()));
    storage->registerResourceData(temporaryFilePath, ioDevice);
    storage->setIsIoDeviceOwner(false);

    using namespace nx::mediaserver_core::plugins;
    auto aviDelegate = std::make_unique<QnAviArchiveDelegate>();
    aviDelegate->setStorage(storage);
    aviDelegate->setAudioChannel(0);
    aviDelegate->setStartTimeUs(timePeriod.startTimeMs * 1000);
    aviDelegate->setUseAbsolutePos(false);

    std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate = std::move(aviDelegate);
    #if defined(ENABLE_SOFTWARE_MOTION_DETECTION)
        if (m_resource->isRemoteArchiveMotionDetectionEnabled())
        {
            auto motionDelegate = std::make_unique<plugins::MotionDelegateWrapper>(
                std::move(archiveDelegate));

            motionDelegate->setMotionRegion(m_resource->getMotionRegion(0));
            archiveDelegate = std::move(motionDelegate);
        }
    #endif

    QnAviResourcePtr aviResource(new QnAviResource(temporaryFilePath));

    m_archiveReader = std::make_unique<QnArchiveStreamReader>(aviResource);
    m_archiveReader->setObjectName(kReaderThreadName);
    m_archiveReader->setArchiveDelegate(archiveDelegate.release());
    m_archiveReader->setPlaybackMask(timePeriod);

    m_archiveReader->setErrorHandler(
        [this, timePeriod](const QString& errorString)
        {
            NX_DEBUG(
                this,
                lm("Can not synchronize time period: %1-%2, error: %3")
                    .args(timePeriod.startTimeMs, timePeriod.endTimeMs(), errorString));

            qnEventRuleConnector->at_remoteArchiveSyncError(
                m_resource,
                errorString);

            m_archiveReader->pleaseStop();

            NX_ASSERT(m_recorder);
            if (m_recorder)
                m_recorder->pleaseStop();
        });

    m_archiveReader->setNoDataHandler(
        [this]()
        {
            m_archiveReader->pleaseStop();

            NX_ASSERT(m_recorder, lit("Recorder should exist."));
            if (m_recorder)
                m_recorder->pleaseStop();
        });
}

void RemoteArchiveSynchronizationTask::createStreamRecorderThreadUnsafe(
    const QnTimePeriod& timePeriod,
    const nx::core::resource::RemoteArchiveChunk& chunk)
{
    using namespace std::chrono;

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
        [this](const QnConstMetaDataV1Ptr& motion){return saveMotion(motion);});

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
                (double) duration_cast<milliseconds>(m_importedDuration).count()
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

bool RemoteArchiveSynchronizationTask::needToReportProgress() const
{
    return true; //< report each written file.
}

boost::optional<RemoteArchiveChunk>
RemoteArchiveSynchronizationTask::remoteArchiveChunkByTimePeriod(const QnTimePeriod& timePeriod)
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
        return boost::none;

    return *chunkItr;
}

bool RemoteArchiveSynchronizationTask::saveMotion(const QnConstMetaDataV1Ptr& motion)
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

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

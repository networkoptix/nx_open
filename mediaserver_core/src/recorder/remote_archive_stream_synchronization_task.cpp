#include "remote_archive_stream_synchronization_task.h"

#include <core/resource/security_cam_resource.h>

#include <recorder/storage_manager.h>

#include <nx/utils/log/log.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/mediaserver/event/event_connector.h>

#include <utils/common/util.h>
#include <utils/common/sleep.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const std::chrono::milliseconds kDetalizationLevel(1);
static const std::chrono::milliseconds kMinChunkDuration(1000);

} // namespace

RemoteArchiveStreamSynchronizationTask::RemoteArchiveStreamSynchronizationTask(
    QnMediaServerModule* serverModule,
    const QnSecurityCamResourcePtr& resource)
    :
    AbstractRemoteArchiveSynchronizationTask(serverModule),
    m_resource(resource)
{
}

void RemoteArchiveStreamSynchronizationTask::setDoneHandler(std::function<void()> handler)
{
    m_doneHandler = handler;
}

void RemoteArchiveStreamSynchronizationTask::cancel()
{
    QnMutexLocker lock(&m_mutex);
    m_canceled = true;

    if (m_archiveReader)
        m_archiveReader->pleaseStop();

    if (m_recorder)
        m_recorder->pleaseStop();
}

bool RemoteArchiveStreamSynchronizationTask::execute()
{
    return synchronizeArchive();
}

bool RemoteArchiveStreamSynchronizationTask::synchronizeArchive()
{
    NX_INFO(
        this,
        lm("Starting archive synchronization. Resource: %1 %2")
            .args(m_resource->getName(), m_resource->getUniqueId()));

    auto manager = m_resource->remoteArchiveManager();
    if (!manager)
    {
        NX_ERROR(
            this,
            lm("Resource %1 %2 has no remote archive manager")
                .args(m_resource->getName(), m_resource->getUniqueId()));

        return false;
    }

    std::vector<RemoteArchiveChunk> deviceChunks;
    auto result = manager->listAvailableArchiveEntries(&deviceChunks);
    if (!result)
    {
        NX_ERROR(
            this,
            lm("Can not fetch chunks from camera %1 %2")
                .args(m_resource->getName(), m_resource->getUniqueId()));

        return false;
    }

    if (deviceChunks.empty())
        return true;

    const auto lastDeviceChunkIndex = deviceChunks.size() - 1;
    const auto startTimeMs = deviceChunks[0].startTimeMs;
    const auto endTimeMs = deviceChunks[lastDeviceChunkIndex].startTimeMs
        + deviceChunks[lastDeviceChunkIndex].durationMs;

    auto serverTimePeriods = qnNormalStorageMan
        ->getFileCatalog(m_resource->getUniqueId(), QnServer::ChunksCatalog::HiQualityCatalog)
        ->getTimePeriods(
            startTimeMs,
            endTimeMs,
            kDetalizationLevel.count(),
            false,
            std::numeric_limits<int>::max());

    auto deviceTimePeriods = toTimePeriodList(deviceChunks);
    NX_DEBUG(this, lm("Device time periods: %1").arg(deviceTimePeriods));
    NX_DEBUG(this, lm("Server time periods: %1").arg(serverTimePeriods));

    deviceTimePeriods.excludeTimePeriods(serverTimePeriods);
    NX_DEBUG(this, lm("Periods to import from device: %1").arg(deviceTimePeriods));

    if (deviceTimePeriods.isEmpty())
        return true;

    m_totalDuration = totalDuration(deviceTimePeriods);
    NX_DEBUG(
        this,
        lm("Total remote archive length to synchronize: %1")
            .arg(m_totalDuration.count()));

    qnEventRuleConnector->at_remoteArchiveSyncStarted(m_resource);
    for (const auto& timePeriod: deviceTimePeriods)
    {
        if (m_canceled)
            break;

        if (timePeriod.durationMs >= kMinChunkDuration.count())
        {
            QnSleep::msleep(3000); //< Without that sleep Hanwha cameras fail and go offline.
            writeTimePeriodToArchive(timePeriod);
        }
    }

    qnEventRuleConnector->at_remoteArchiveSyncFinished(m_resource);
    return true;
}

bool RemoteArchiveStreamSynchronizationTask::writeTimePeriodToArchive(
    const QnTimePeriod& timePeriod)
{
    if (timePeriod.isInfinite())
        return false;

    const std::chrono::milliseconds startTime(timePeriod.startTimeMs);
    const std::chrono::milliseconds endTime(timePeriod.endTimeMs());

    if (endTime <= startTime)
        return false;

    {
        QnMutexLocker lock(&m_mutex);
        resetRecorderUnsafe(startTime, endTime);
        resetArchiveReaderUnsafe(startTime, endTime);
    }

    NX_ASSERT(m_recorder && m_archiveReader);
    if (!m_recorder || !m_archiveReader)
        return false;

    m_recorder->start();
    m_archiveReader->start();

    m_recorder->wait();
    m_archiveReader->wait();

    m_importedDuration += std::chrono::milliseconds(timePeriod.durationMs);

    return true;
}

void RemoteArchiveStreamSynchronizationTask::resetArchiveReaderUnsafe(
    const std::chrono::milliseconds& startTime,
    const std::chrono::milliseconds& endTime)
{
    using namespace std::chrono;

    NX_ASSERT(endTime > startTime);
    if (endTime <= startTime)
        return;

    auto archiveDelegate = m_resource
        ->remoteArchiveManager()
        ->archiveDelegate();

    if (!archiveDelegate)
        return;

    archiveDelegate->setPlaybackMode(PlaybackMode::Edge);
    archiveDelegate->setRange(
        duration_cast<microseconds>(startTime).count(),
        duration_cast<microseconds>(endTime).count(),
        /*frameStep*/ 1);

    m_archiveReader = std::make_unique<QnArchiveStreamReader>(m_resource);
    m_archiveReader->setArchiveDelegate(archiveDelegate.release());
    m_archiveReader->setPlaybackRange(QnTimePeriod(startTime, endTime - startTime));
    m_archiveReader->setRole(Qn::CR_Archive);
    m_archiveReader->addDataProcessor(m_recorder.get());

    m_archiveReader->setErrorHandler(
        [this, startTime, endTime](const QString& errorString)
        {
            NX_DEBUG(
                this,
                lm("Can not synchronize time period: %1-%2, error: %3")
                    .args(startTime, endTime, errorString));

            qnEventRuleConnector->at_remoteArchiveSyncError(
                m_resource,
                errorString);

            m_archiveReader->pleaseStop();

            NX_ASSERT(m_recorder);
            if (m_recorder)
                m_recorder->pleaseStop();
        });
}

void RemoteArchiveStreamSynchronizationTask::resetRecorderUnsafe(
    const std::chrono::milliseconds& startTime,
    const std::chrono::milliseconds& endTime)
{
    using namespace std::chrono;

    auto saveMotionHandler = [](const QnConstMetaDataV1Ptr& motion) { return false; };

    m_recorder = std::make_unique<QnServerEdgeStreamRecorder>(
        m_resource,
        QnServer::ChunksCatalog::HiQualityCatalog,
        m_archiveReader.get());

    m_recorder->setSaveMotionHandler(saveMotionHandler);
    m_recorder->setRecordingBounds(
        duration_cast<microseconds>(startTime),
        duration_cast<microseconds>(endTime));

    m_recorder->setOnFileWrittenHandler(
        [this](
            std::chrono::milliseconds /*startTime*/,
            std::chrono::milliseconds duration)
        {
            m_importedDuration += duration;
            if (needToReportProgress())
            {
                auto progress = (double)m_importedDuration.count() / m_totalDuration.count();
                if (progress > 1.0)
                {
                    progress = 1.0;
                    NX_DEBUG(
                        this,
                        lm("Wrong progress! Imported: %1, Total: %2")
                            .args(m_importedDuration.count(), m_totalDuration.count()));
                }

                qnEventRuleConnector->at_remoteArchiveSyncProgress(
                    m_resource,
                    progress);
            }
        });

    m_recorder->setEndOfRecordingHandler(
        [this]()
        {
            NX_VERBOSE(this, lit("Stopping recording from recorder. Got out of bounds packet."));
            if (m_archiveReader)
                m_archiveReader->pleaseStop();

            m_recorder->pleaseStop();
        });
}

QnTimePeriodList RemoteArchiveStreamSynchronizationTask::toTimePeriodList(
    const std::vector<RemoteArchiveChunk>& chunks)
{
    QnTimePeriodList result;
    for (const auto& chunk: chunks)
    {
        if (chunk.durationMs > 0)
            result << QnTimePeriod(chunk.startTimeMs, chunk.durationMs);
    }

    return result;
}

std::chrono::milliseconds RemoteArchiveStreamSynchronizationTask::totalDuration(
    const QnTimePeriodList& deviceChunks)
{
    int64_t result = 0;
    for (const auto& chunk: deviceChunks)
    {
        NX_ASSERT(!chunk.isInfinite());
        if (chunk.isInfinite())
            continue;

        if (chunk.durationMs < kMinChunkDuration.count())
            continue;

        result += chunk.durationMs;
    }

    return std::chrono::milliseconds(result);
}

bool RemoteArchiveStreamSynchronizationTask::needToReportProgress() const
{
    return true; //< For now let's report progress for each recorded file.
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

#include "remote_archive_stream_synchronization_task.h"

#include <core/resource/security_cam_resource.h>

#include <recording/stream_recorder.h>

#include <recorder/storage_manager.h>

#include <nx/utils/log/log.h>

#include <utils/common/util.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const int kDetailLevelMs = 1000;

} // namespace

RemoteArchiveStreamSynchronizationTask::RemoteArchiveStreamSynchronizationTask(
    QnCommonModule* commonModule)
{

}

void RemoteArchiveStreamSynchronizationTask::setResource(const QnSecurityCamResourcePtr& resource)
{
    m_resource = resource;
}

void RemoteArchiveStreamSynchronizationTask::setDoneHandler(std::function<void()> handler)
{
    m_doneHandler = handler;
}

void RemoteArchiveStreamSynchronizationTask::cancel()
{
    m_canceled = true;
}

bool RemoteArchiveStreamSynchronizationTask::execute()
{
    return synchronizeArchive();
}

bool RemoteArchiveStreamSynchronizationTask::synchronizeArchive()
{
    NX_LOGX(
        lm("Starting archive synchronization. Resource: %1 %2")
            .arg(m_resource->getName())
            .arg(m_resource->getUniqueId()),
        cl_logINFO);

    auto manager = m_resource->remoteArchiveManager();
    if (!manager)
    {
        NX_LOGX(
            lm("Resource %1 %2 has no remote archive manager")
                .arg(m_resource->getName())
                .arg(m_resource->getUniqueId()),
            cl_logWARNING);

        return false;
    }

    std::vector<RemoteArchiveEntry> deviceChunks;
    auto result = manager->listAvailableArchiveEntries(&deviceChunks);
    if (!result)
    {
        NX_LOGX(
            lm("Can not fetch chunks from camera %1 %2")
                .arg(m_resource->getName())
                .arg(m_resource->getUniqueId()),
            cl_logWARNING);

        return false;
    }

    if (deviceChunks.empty())
        return true;

    const auto lastDeviceChunkIndex = deviceChunks.size() - 1;
    const auto startTimeMs = deviceChunks[0].startTimeMs;
    const auto endTimeMs = deviceChunks[lastDeviceChunkIndex].startTimeMs
        + deviceChunks[lastDeviceChunkIndex].durationMs;

    auto serverChunks = qnNormalStorageMan
        ->getFileCatalog(m_resource->getUniqueId(), QnServer::ChunksCatalog::HiQualityCatalog)
        ->getTimePeriods(
            startTimeMs,
            endTimeMs,
            kDetailLevelMs,
            false,
            std::numeric_limits<int>::max());

    // TODO: calculate difference somehow
    for (const auto& chunk: deviceChunks)
        writeEntryToArchive(chunk);

    return true;
}

bool RemoteArchiveStreamSynchronizationTask::writeEntryToArchive(const RemoteArchiveEntry& entry)
{
    auto reader = archiveReader(entry.startTimeMs, entry.startTimeMs + entry.durationMs);
    reader->setPlaybackRange(QnTimePeriod(entry.startTimeMs, entry.durationMs));

    auto saveMotionHandler = [](const QnConstMetaDataV1Ptr& motion) { return false; };
    const auto fileName = chunkFileName(entry);
    QFileInfo info(fileName);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath("."))
        return false;

    QnStreamRecorder recorder(m_resource);
    recorder.clearUnprocessedData();
    recorder.addRecordingContext(
        fileName,
        qnNormalStorageMan->getOptimalStorageRoot());

    recorder.setContainer(kArchiveContainer);
    recorder.disableRegisterFile(true);
    recorder.setSaveMotionHandler(saveMotionHandler);

    qnNormalStorageMan->fileStarted(
        entry.startTimeMs,
        currentTimeZone() / 60,
        fileName,
        nullptr);

    reader->addDataProcessor(&recorder);
    reader->start();
    reader->wait();

    recorder.close(); //< Do we need it?

    return qnNormalStorageMan->fileFinished(
        recorder.duration() / 1000,
        fileName,
        nullptr,
        recorder.lastFileSize());
}

QnAbstractArchiveStreamReader* RemoteArchiveStreamSynchronizationTask::archiveReader(
    int64_t startTimeMs,
    int64_t endTimeMs)
{
    if (!m_reader)
    {
        auto delegate = m_resource
            ->remoteArchiveManager()
            ->archiveDelegate();

        if (!delegate)
            return nullptr;

        m_reader = std::make_unique<QnArchiveStreamReader>(m_resource);
        m_reader->setArchiveDelegate(delegate);
    }

    auto delegate = m_reader->getArchiveDelegate();
    delegate->setRange(startTimeMs, endTimeMs, /*frameStep*/1);

    return m_reader.get();
}

QString RemoteArchiveStreamSynchronizationTask::chunkFileName(
    const RemoteArchiveEntry& entry) const
{
    auto filename = qnNormalStorageMan->getFileName(
        entry.startTimeMs,
        currentTimeZone() / 60,
        m_resource,
        DeviceFileCatalog::prefixByCatalog(QnServer::ChunksCatalog::HiQualityCatalog),
        qnNormalStorageMan->getOptimalStorageRoot());

    return filename + kArchiveContainerExtension;
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

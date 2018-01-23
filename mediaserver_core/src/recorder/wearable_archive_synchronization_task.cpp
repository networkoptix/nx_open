#include "wearable_archive_synchronization_task.h"

#include <nx/utils/random.h>
#include <nx/utils/log/log.h>
#include <nx/streaming/archive_stream_reader.h>
#include <plugins/storage/memory/ext_iodevice_storage.h>
#include <plugins/resource/avi/avi_archive_delegate.h>
#include <plugins/resource/avi/avi_resource.h>
#include <core/resource/security_cam_resource.h>

#include "server_edge_stream_recorder.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {


WearableArchiveSynchronizationTask::WearableArchiveSynchronizationTask(
    QnMediaServerModule* serverModule,
    const QnUuid& id,
    const QnSecurityCamResourcePtr& resource,
    std::unique_ptr<QIODevice> file,
    qint64 startTimeMs)
    :
    base_type(serverModule),
    m_resource(resource),
    m_file(file.release()),
    m_startTimeMs(startTimeMs)
{}

WearableArchiveSynchronizationTask::~WearableArchiveSynchronizationTask()
{
    // Actual data should be deleted in execute().
    NX_ASSERT(!m_file);
    if (m_file)
        delete m_file.data();
}

QnUuid WearableArchiveSynchronizationTask::id() const
{
    return m_id;
}

void WearableArchiveSynchronizationTask::setDoneHandler(std::function<void()> handler)
{
    m_doneHandler = handler;
}

void WearableArchiveSynchronizationTask::cancel()
{
    if (m_archiveReader)
        m_archiveReader->pleaseStop();

    if (m_recorder)
        m_recorder->pleaseStop();
}

bool WearableArchiveSynchronizationTask::execute()
{
    createArchiveReader(m_startTimeMs);
    createStreamRecorder(m_startTimeMs);

    m_archiveReader->addDataProcessor(m_recorder.get());
    m_recorder->start();
    m_archiveReader->start();

    m_recorder->wait();
    m_archiveReader->wait();

    m_recorder.reset();
    m_archiveReader.reset();

    setProgress(100);

    if(m_doneHandler)
        m_doneHandler();

    return true;
}

int WearableArchiveSynchronizationTask::progress() const
{
    return m_progress;
}

void WearableArchiveSynchronizationTask::setProgress(int progress)
{
    bool changed = m_progress != progress;

    m_progress = progress;

    if (changed)
        emit progressChanged(progress);
}

void WearableArchiveSynchronizationTask::createArchiveReader(qint64 startTimeMs)
{
    QString temporaryFilePath = QString::number(nx::utils::random::number());
    QnExtIODeviceStorageResourcePtr storage(new QnExtIODeviceStorageResource(commonModule()));
    // Storage takes ownership on file data.
    storage->registerResourceData(temporaryFilePath, m_file.data());
    storage->setIsIoDeviceOwner(false);

    using namespace nx::mediaserver_core::plugins;
    std::unique_ptr<QnAviArchiveDelegate> delegate = std::make_unique<QnAviArchiveDelegate>();
    delegate->setStorage(storage);
    delegate->setAudioChannel(0);
    delegate->setStartTimeUs(startTimeMs * 1000);
    delegate->setUseAbsolutePos(false);

    QnAviResourcePtr aviResource(new QnAviResource(temporaryFilePath));

    m_archiveReader = std::make_unique<QnArchiveStreamReader>(aviResource);
    m_archiveReader->setObjectName(lit("WearableCameraArchiveReader"));
    m_archiveReader->setArchiveDelegate(delegate.release());
    //m_archiveReader->setPlaybackMask(timePeriod);

    m_archiveReader->setErrorHandler(
        [this](const QString& errorString)
        {
            NX_DEBUG(this, lm("Can not synchronize wearable chunk, error: %1").args(errorString));

            m_archiveReader->pleaseStop();
            if (m_recorder)
                m_recorder->pleaseStop();
            setProgress(-1);
        });

    m_archiveReader->setNoDataHandler(
        [this]()
        {
            m_archiveReader->pleaseStop();
            if (m_recorder)
                m_recorder->pleaseStop();
            setProgress(99);
        });
}

void WearableArchiveSynchronizationTask::createStreamRecorder(qint64 startTimeMs)
{
    using namespace std::chrono;

    NX_ASSERT(m_archiveReader, lit("Archive reader should be created before stream recorder"));

    m_recorder = std::make_unique<QnServerEdgeStreamRecorder>(
        m_resource,
        QnServer::ChunksCatalog::HiQualityCatalog,
        m_archiveReader.get());

    auto saveMotionHandler = [](const QnConstMetaDataV1Ptr& motion) { return false; };
    m_recorder->setSaveMotionHandler(saveMotionHandler);
    m_recorder->setObjectName(lit("WearableCameraArchiveRecorder"));

    m_recorder->setOnFileWrittenHandler(
        [this](milliseconds startTime, milliseconds duration)
        {
            if (!m_archiveReader)
                return;

            QnAbstractArchiveDelegate* delegate = m_archiveReader->getArchiveDelegate();

            qint64 globalStartMs = delegate->startTime() / 1000;
            qint64 globalDurationMs = (delegate->endTime() - delegate->startTime()) / 1000;
            qint64 progressMs = (startTime + duration).count() - globalStartMs;

            int progress = progressMs * 100 / globalDurationMs;
            setProgress(std::max(m_progress, progress == 100 ? 99 : progress));
        });

    m_recorder->setEndOfRecordingHandler(
        [this]()
        {
            m_recorder->pleaseStop();
            if (m_archiveReader)
                m_archiveReader->pleaseStop();
            setProgress(99);
        });
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

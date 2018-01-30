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
    const QnSecurityCamResourcePtr& resource,
    std::unique_ptr<QIODevice> file,
    qint64 startTimeMs)
    :
    base_type(serverModule),
    m_resource(resource),
    m_file(file.release()),
    m_startTimeMs(startTimeMs)
{
    qRegisterMetaType<WearableArchiveSynchronizationState>("WearableArchiveSynchronizationState");

    m_state.status = WearableArchiveSynchronizationState::Consuming;
}

WearableArchiveSynchronizationTask::~WearableArchiveSynchronizationTask()
{
    // Actual data should be deleted in execute().
    NX_ASSERT(!m_file);
    if (m_file)
        delete m_file.data();
}

QnUuid WearableArchiveSynchronizationTask::id() const
{
    return m_resource->getId();
}

void WearableArchiveSynchronizationTask::setDoneHandler(nx::utils::MoveOnlyFunc<void()> /*handler*/)
{
    NX_ASSERT(false);
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

    m_state.status = WearableArchiveSynchronizationState::Finished;
    emit stateChanged(m_state);

    return true;
}

WearableArchiveSynchronizationState WearableArchiveSynchronizationTask::state() const
{
    return m_state;
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

    QnAviResourcePtr resource(new QnAviResource(temporaryFilePath));
    delegate->open(resource);
    delegate->findStreams();

    qint64 duration = 0;
    if(delegate->endTime() != AV_NOPTS_VALUE)
        duration = (delegate->endTime() - delegate->startTime()) / 1000;

    m_archiveReader = std::make_unique<QnArchiveStreamReader>(resource);
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

            m_state.errorString = errorString;
        });

    m_archiveReader->setNoDataHandler(
        [this]()
        {
            m_archiveReader->pleaseStop();
            if (m_recorder)
                m_recorder->pleaseStop();

            m_state.processed = m_state.duration;
        });

    m_state.duration = duration;
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
            m_state.processed = std::min(m_state.duration, m_state.processed + duration.count());
            emit stateChanged(m_state);
        });

    m_recorder->setEndOfRecordingHandler(
        [this]()
        {
            m_recorder->pleaseStop();
            if (m_archiveReader)
                m_archiveReader->pleaseStop();

            m_state.processed = m_state.duration;
        });
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

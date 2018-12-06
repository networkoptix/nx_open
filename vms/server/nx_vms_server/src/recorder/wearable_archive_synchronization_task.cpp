#include "wearable_archive_synchronization_task.h"

#include <utils/common/sleep.h>
#include <nx/utils/random.h>
#include <nx/utils/log/log.h>
#include <nx/streaming/archive_stream_reader.h>
#include <core/storage/memory/ext_iodevice_storage.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/avi/avi_resource.h>
#include <plugins/utils/avi_motion_archive_delegate.h>
#include <core/resource/security_cam_resource.h>

#include "server_edge_stream_recorder.h"

#include <nx/utils/std/cpp14.h>
#include <media_server/media_server_module.h>

namespace nx {
namespace vms::server {
namespace recorder {

using namespace plugins;


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
    // execute was never called?
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
    m_withMotion = m_resource->getMotionType() == Qn::MotionType::MT_SoftwareGrid;

    createArchiveReader(m_startTimeMs, &m_state.duration);
    createStreamRecorder(m_startTimeMs, m_state.duration);
    m_archiveReader->addDataProcessor(m_recorder.get());

    m_recorder->start();
    m_archiveReader->start();
    m_archiveReader->wait();

    // Now some bad news: recorder might still have some packets in queue.
    // And there is no way to wait until it's done. The workaround here is questionable at best.
    while(m_recorder->queueSize() > 0)
        QnSleep::msleep(10);
    m_recorder->pleaseStop();
    m_recorder->wait();

    m_archiveReader.reset();
    m_recorder.reset();

    m_state.processed = m_state.duration;
    m_state.status = WearableArchiveSynchronizationState::Finished;
    emit stateChanged(m_state);

    return true;
}

WearableArchiveSynchronizationState WearableArchiveSynchronizationTask::state() const
{
    return m_state;
}

QnAviArchiveDelegate* WearableArchiveSynchronizationTask::createArchiveDelegate()
{
    if (!m_withMotion)
        return new QnAviArchiveDelegate();

    std::unique_ptr<AviMotionArchiveDelegate> result = std::make_unique<AviMotionArchiveDelegate>();
    QnMotionRegion region;
    result->setMotionRegion(m_resource->getMotionRegion(0));
    return result.release();
}

void WearableArchiveSynchronizationTask::createArchiveReader(qint64 startTimeMs, qint64* durationMs)
{
    QString temporaryFilePath = QString::number(nx::utils::random::number());
    QnExtIODeviceStorageResourcePtr storage(new QnExtIODeviceStorageResource(serverModule()->commonModule()));
    // Storage takes ownership on file data.
    storage->registerResourceData(temporaryFilePath, m_file.data());
    storage->setIsIoDeviceOwner(false);

    using namespace nx::vms::server::plugins;
    std::unique_ptr<QnAviArchiveDelegate> delegate(createArchiveDelegate());
    delegate->setStorage(storage);
    delegate->setAudioChannel(0);
    delegate->setStartTimeUs(startTimeMs * 1000);
    delegate->setUseAbsolutePos(false);

    QnAviResourcePtr resource(new QnAviResource(temporaryFilePath));
    delegate->open(resource, nullptr);
    delegate->findStreams();

    *durationMs = 0;
    if(delegate->endTime() != AV_NOPTS_VALUE)
        *durationMs = (delegate->endTime() - delegate->startTime()) / 1000;

    m_archiveReader = std::make_unique<QnArchiveStreamReader>(m_resource);
    m_archiveReader->setObjectName(lit("WearableCameraArchiveReader"));
    m_archiveReader->setArchiveDelegate(delegate.release());
    //m_archiveReader->setPlaybackMask(timePeriod);

    m_archiveReader->setErrorHandler(
        [this](const QString& errorString)
        {
            NX_DEBUG(this, lm("Could not synchronize wearable chunk, error: %1").args(errorString));

            m_archiveReader->pleaseStop();

            QnMutexLocker locker(&m_stateMutex);
            m_state.errorString = errorString;
        });

    m_archiveReader->setNoDataHandler(
        [this]()
        {
            m_archiveReader->pleaseStop();
        });
}

void WearableArchiveSynchronizationTask::createStreamRecorder(qint64 startTimeMs, qint64 durationMs)
{
    using namespace std::chrono;

    NX_ASSERT(m_archiveReader, lit("Archive reader should be created before stream recorder"));

    m_recorder = std::make_unique<QnServerEdgeStreamRecorder>(
        serverModule(),
        m_resource,
        QnServer::ChunksCatalog::HiQualityCatalog,
        m_archiveReader.get());

    if (!m_withMotion)
    {
        auto saveMotionHandler = [](const QnConstMetaDataV1Ptr& motion) { return false; };
        m_recorder->setSaveMotionHandler(saveMotionHandler);
    }

    m_recorder->setObjectName(lit("WearableCameraArchiveRecorder"));

    // Make sure we get recordingProgress notifications.
    // Adding 100ms at end just to feel safe. We don't want to drop packets.
    m_recorder->setProgressBounds(startTimeMs * 1000, (startTimeMs + durationMs + 100) * 1000);

    // Note that direct connection is OK here as we control the
    // lifetimes of all the objects involved.
    connect(m_recorder, &QnServerStreamRecorder::recordingProgress, this,
        [this](int progress)
        {
            QnMutexLocker locker(&m_stateMutex);
            m_state.processed = m_state.duration * progress / 100;
            locker.unlock();

            emit stateChanged(m_state);
        }, Qt::DirectConnection);
}

} // namespace recorder
} // namespace vms::server
} // namespace nx

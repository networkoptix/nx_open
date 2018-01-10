#include "wearable_archive_synchronization_task.h"

#include <nx/utils/random.h>
#include <nx/utils/log/log.h>
#include <nx/streaming/archive_stream_reader.h>
#include <plugins/storage/memory/ext_iodevice_storage.h>
#include <plugins/resource/avi/avi_archive_delegate.h>
#include <plugins/resource/avi/avi_resource.h>
#include <core/resource/security_cam_resource.h>

#include "server_edge_stream_recorder.h"

namespace nx {
namespace mediaserver_core {
namespace recorder {


WearableArchiveSynchronizationTask::WearableArchiveSynchronizationTask(
    QnCommonModule* commonModule,
    const QnSecurityCamResourcePtr& resource,
    QIODevice* file,
    qint64 startTimeMs
): 
    base_type(commonModule),
    m_resource(resource),
    m_file(file),
    m_startTimeMs(startTimeMs)
{}

WearableArchiveSynchronizationTask::~WearableArchiveSynchronizationTask() 
{
    if (m_file)
        delete m_file.data(); /* execute was never called / something went terribly wrong. */
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

    return true;
}

void WearableArchiveSynchronizationTask::createArchiveReader(qint64 startTimeMs) 
{
    QString temporaryFilePath = QString::number(nx::utils::random::number());
    QnExtIODeviceStorageResourcePtr storage(new QnExtIODeviceStorageResource(commonModule()));
    storage->registerResourceData(temporaryFilePath, m_file.data());
    storage->setIsIoDeviceOwner(false);

    using namespace nx::mediaserver_core::plugins;
    auto archiveDelegate = new QnAviArchiveDelegate();
    archiveDelegate->setStorage(storage);
    archiveDelegate->setAudioChannel(0);
    archiveDelegate->setStartTimeUs(startTimeMs * 1000);
    archiveDelegate->setUseAbsolutePos(false);

    QnAviResourcePtr aviResource(new QnAviResource(temporaryFilePath));

    m_archiveReader = std::make_unique<QnArchiveStreamReader>(aviResource);
    m_archiveReader->setObjectName(lit("WearableCameraArchiveReader"));
    m_archiveReader->setArchiveDelegate(archiveDelegate);
    //m_archiveReader->setPlaybackMask(timePeriod);

    m_archiveReader->setErrorHandler([this](const QString& errorString) {
        NX_DEBUG(this, lm("Can not synchronize wearable chunk, error: %1").args(errorString));

        m_archiveReader->pleaseStop();
        if (m_recorder)
            m_recorder->pleaseStop();
    });

    m_archiveReader->setNoDataHandler([this]() {
        m_archiveReader->pleaseStop();
        if (m_recorder)
            m_recorder->pleaseStop();
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

    m_recorder->setOnFileWrittenHandler([this](milliseconds startTime, milliseconds duration) {
        int a = 10;
    });

    m_recorder->setEndOfRecordingHandler([this]() {
        if (m_archiveReader)
            m_archiveReader->pleaseStop();

        m_recorder->pleaseStop();
    });
}


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

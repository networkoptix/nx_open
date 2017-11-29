#include "remote_archive_synchronization_task.h"
#include "remote_archive_common.h"

#include <motion/motion_helper.h>

#include <plugins/resource/avi/avi_archive_delegate.h>
#include <plugins/storage/memory/ext_iodevice_storage.h>
#include <plugins/utils/motion_delegate_wrapper.h>
#include <plugins/resource/avi/avi_resource.h>

#include <nx/mediaserver/event/event_connector.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/random.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const QString kReaderThreadName = lit("Edge reader");

} // namespace

using namespace nx::core::resource;
using namespace std::chrono;

RemoteArchiveSynchronizationTask::RemoteArchiveSynchronizationTask(
    QnMediaServerModule* serverModule,
    const QnSecurityCamResourcePtr& resource)
    :
    base_type(serverModule, resource)
{
}

void RemoteArchiveSynchronizationTask::createArchiveReaderThreadUnsafe(
    const QnTimePeriod& timePeriod)
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

bool RemoteArchiveSynchronizationTask::prepareDataSource(
    const QnTimePeriod& timePeriod,
    const nx::core::resource::RemoteArchiveChunk& chunk)
{
    auto manager = m_resource->remoteArchiveManager();
    if (!manager)
    {
        NX_ERROR(
            this,
            lm("Resource %1 %2 has no remote archive manager")
            .args(m_resource->getUserDefinedName(), m_resource->getUniqueId()));

        return false;
    }

    if (chunk == m_currentChunk)
        return true;

    m_currentChunk = chunk;
    m_currentChunkBuffer.clear();
    if (!manager->fetchArchiveEntry(chunk.id, &m_currentChunkBuffer))
    {
        NX_DEBUG(
            this,
            lm("Can not fetch archive chunk %1. Resource %2")
                .args(chunk.id, m_resource->getUserDefinedName()));

        return false;
    }

    NX_DEBUG(
        this,
        lm("Archive chunk with id %1 has been fetched, size %2 bytes")
            .args(chunk.id, m_currentChunkBuffer.size()));

    return true;
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

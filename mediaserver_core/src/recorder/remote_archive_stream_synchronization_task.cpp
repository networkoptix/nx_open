#include "remote_archive_stream_synchronization_task.h"

#include <core/resource/security_cam_resource.h>

#include <recorder/storage_manager.h>
#include <plugins/utils/motion_delegate_wrapper.h>
#include <motion/motion_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/mediaserver/event/event_connector.h>

#include <utils/common/util.h>
#include <utils/common/synctime.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const QString kReaderThreadName = lit("Edge reader");

} // namespace

using namespace std::chrono;

RemoteArchiveStreamSynchronizationTask::RemoteArchiveStreamSynchronizationTask(
    QnMediaServerModule* serverModule,
    const QnSecurityCamResourcePtr& resource)
    :
    base_type(serverModule, resource)
{
}

void RemoteArchiveStreamSynchronizationTask::createArchiveReaderThreadUnsafe(
    const QnTimePeriod& timePeriod)
{
    auto archiveDelegate = m_resource
        ->remoteArchiveManager()
        ->archiveDelegate();

    if (!archiveDelegate)
        return;

    archiveDelegate->setPlaybackMode(PlaybackMode::Edge);
    archiveDelegate->setRange(
        duration_cast<microseconds>(milliseconds(timePeriod.startTimeMs)).count(),
        duration_cast<microseconds>(milliseconds(timePeriod.endTimeMs())).count(),
        /*frameStep*/ 1);

    #if defined(ENABLE_SOFTWARE_MOTION_DETECTION)
        if (m_resource->isRemoteArchiveMotionDetectionEnabled())
        {
            auto motionDelegate = std::make_unique<plugins::MotionDelegateWrapper>(
                std::move(archiveDelegate));

            motionDelegate->setMotionRegion(m_resource->getMotionRegion(0));
            archiveDelegate = std::move(motionDelegate);
        }
    #endif

    m_archiveReader = std::make_unique<QnArchiveStreamReader>(m_resource);
    m_archiveReader->setObjectName(kReaderThreadName);
    m_archiveReader->setArchiveDelegate(archiveDelegate.release());
    m_archiveReader->setPlaybackRange(timePeriod);
    m_archiveReader->setRole(Qn::CR_Archive);

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
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

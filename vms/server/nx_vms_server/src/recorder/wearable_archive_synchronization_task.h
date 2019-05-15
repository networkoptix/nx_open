#pragma once

#include <QtCore/QPointer>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/server/server_module_aware.h>

#include "abstract_remote_archive_synchronization_task.h"
#include "wearable_archive_synchronization_state.h"

class QIODevice;

class QnAbstractArchiveStreamReader;
class QnServerEdgeStreamRecorder;
class QnAviArchiveDelegate;

namespace nx {
namespace vms::server {
namespace recorder {

/**
 * Synchronization task for wearable cameras.
 * Basically just writes a single file into archive.
 */
class WearableArchiveSynchronizationTask:
    public Connective<QObject>,
    public AbstractRemoteArchiveSynchronizationTask
{
    Q_OBJECT
    using base_type = AbstractRemoteArchiveSynchronizationTask;

public:
    WearableArchiveSynchronizationTask(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& resource,
        std::unique_ptr<QIODevice> file,
        qint64 startTimeMs
    );
    virtual ~WearableArchiveSynchronizationTask() override;

    virtual QnUuid id() const override;
    virtual void setDoneHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;

    WearableArchiveSynchronizationState state() const;

signals:
    void stateChanged(WearableArchiveSynchronizationState state);

private:
    QnAviArchiveDelegate* createArchiveDelegate();
    void createArchiveReader(qint64 startTimeMs, qint64* durationMs);
    void createStreamRecorder(qint64 startTimeMs, qint64 durationMs);

private:
    QnSecurityCamResourcePtr m_camera;
    QPointer<QIODevice> m_file;
    qint64 m_startTimeMs = 0;

    std::unique_ptr<QnAbstractArchiveStreamReader> m_archiveReader;
    std::unique_ptr<QnServerEdgeStreamRecorder> m_recorder;
    bool m_withMotion = false;

    QnMutex m_stateMutex;
    WearableArchiveSynchronizationState m_state;
};

using WearableArchiveTaskPtr = std::shared_ptr<WearableArchiveSynchronizationTask>;

} // namespace recorder
} // namespace vms::server
} // namespace nx

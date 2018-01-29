#pragma once

#include <QtCore/QPointer>

#include <nx/mediaserver/server_module_aware.h>
#include <core/resource/resource_fwd.h>

#include "abstract_remote_archive_synchronization_task.h"
#include "wearable_archive_synchronization_state.h"

class QIODevice;

class QnAbstractArchiveStreamReader;
class QnServerEdgeStreamRecorder;

namespace nx {
namespace mediaserver_core {
namespace recorder {

/**
 * Synchronization task for wearable cameras.
 * Basically just writes a single file into archive.
 */
class WearableArchiveSynchronizationTask:
    public QObject,
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
    virtual void setDoneHandler(std::function<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;

    WearableArchiveSynchronizationState state() const;

signals:
    void stateChanged(WearableArchiveSynchronizationState state);

private:
    void createArchiveReader(qint64 startTimeMs);
    void createStreamRecorder(qint64 startTimeMs);

private:
    QnSecurityCamResourcePtr m_resource;
    QPointer<QIODevice> m_file;
    qint64 m_startTimeMs = 0;
    WearableArchiveSynchronizationState m_state;

    std::unique_ptr<QnAbstractArchiveStreamReader> m_archiveReader;
    std::unique_ptr<QnServerEdgeStreamRecorder> m_recorder;
};

using WearableArchiveTaskPtr = std::shared_ptr<WearableArchiveSynchronizationTask>;

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

#pragma once

#include <QtCore/QPointer>

#include <nx/mediaserver/server_module_aware.h>
#include <core/resource/resource_fwd.h>

#include "abstract_remote_archive_synchronization_task.h"

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
class WearableArchiveSynchronizationTask: public AbstractRemoteArchiveSynchronizationTask
{
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

private:
    void createArchiveReader(qint64 startTimeMs);
    void createStreamRecorder(qint64 startTimeMs);

private:
    QnSecurityCamResourcePtr m_resource;
    QPointer<QIODevice> m_file;
    qint64 m_startTimeMs;

    std::unique_ptr<QnAbstractArchiveStreamReader> m_archiveReader;
    std::unique_ptr<QnServerEdgeStreamRecorder> m_recorder;
};


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

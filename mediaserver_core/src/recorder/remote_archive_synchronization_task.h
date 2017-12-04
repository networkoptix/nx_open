#pragma once

#include <core/resource/abstract_remote_archive_manager.h>

#include <recorder/base_remote_archive_synchronization_task.h>
#include <nx/network/buffer.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

/*
 * Synchronization task for resources that are able to provide
 * fixed archive chunks only.
 */
class RemoteArchiveSynchronizationTask:
    public BaseRemoteArchiveSynchronizationTask
{
    using base_type = BaseRemoteArchiveSynchronizationTask;
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;

public:
    RemoteArchiveSynchronizationTask(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& resource);

protected:
    virtual void createArchiveReaderThreadUnsafe(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk) override;

    virtual bool prepareDataSource(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk) override;

private:
    nx::core::resource::RemoteArchiveChunk m_currentChunk;
    nx::Buffer m_currentChunkBuffer;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

#pragma once

#include <core/resource/abstract_remote_archive_manager.h>
#include <recorder/base_remote_archive_synchronization_task.h>
#include <recorder/server_edge_stream_recorder.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/abstract_archive_delegate.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

/*
 * Synchronization task for resources that are able to provide
 * archive stream with random access.
 */
class RemoteArchiveStreamSynchronizationTask:
    public BaseRemoteArchiveSynchronizationTask
{
    using base_type = BaseRemoteArchiveSynchronizationTask;
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;
    using RemoteArchiveChunk = nx::core::resource::RemoteArchiveChunk;

public:
    RemoteArchiveStreamSynchronizationTask(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& resource);

    virtual void createArchiveReaderThreadUnsafe(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk) override;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

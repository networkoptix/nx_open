#pragma once

#include <core/resource/abstract_remote_archive_manager.h>

#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <recorder/remote_archive_synchronizer.h>
#include <recorder/server_edge_stream_recorder.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/network/buffer.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

/*
 * Synchronization task for resources that are able to provide
 * fixed archive chunks only.
 */
class RemoteArchiveSynchronizationTask:
    public AbstractRemoteArchiveSynchronizationTask
{
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;

public:
    RemoteArchiveSynchronizationTask(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& resource);

    virtual void setDoneHandler(std::function<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;
    virtual QnUuid id() const override;

private:
    bool synchronizeArchive();

    bool writeTimePeriodToArchive(const QnTimePeriod& timePeriod);

    bool writeTimePeriodInternal(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk);

    boost::optional<nx::core::resource::RemoteArchiveChunk> remoteArchiveChunkByTimePeriod(
        const QnTimePeriod& timePeriod);

    void createArchiveReaderThreadUnsafe(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk);

    void createStreamRecorderThreadUnsafe(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk);

    bool needToReportProgress() const;

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_wait;
    QnSecurityCamResourcePtr m_resource;
    std::atomic<bool> m_canceled{false};
    std::function<void()> m_doneHandler;

    std::chrono::milliseconds m_totalDuration{0};
    std::chrono::milliseconds m_importedDuration{0};

    std::unique_ptr<QnAbstractArchiveStreamReader> m_archiveReader;
    std::unique_ptr<QnServerEdgeStreamRecorder> m_recorder;

    std::vector<nx::core::resource::RemoteArchiveChunk> m_chunks;
    nx::core::resource::RemoteArchiveChunk m_currentChunk;
    nx::Buffer m_currentChunkBuffer;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

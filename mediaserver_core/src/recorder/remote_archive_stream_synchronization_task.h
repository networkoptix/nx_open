#pragma once

#include <core/resource/abstract_remote_archive_manager.h>
#include <recorder/abstract_remote_archive_synchronization_task.h>
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
    public AbstractRemoteArchiveSynchronizationTask
{
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;
    using RemoteArchiveChunk = nx::core::resource::RemoteArchiveChunk;

public:
    RemoteArchiveStreamSynchronizationTask(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& resource);

    virtual void setDoneHandler(std::function<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;
    virtual QnUuid id() const override;

private:
    bool synchronizeArchive();
    bool writeTimePeriodToArchive(const QnTimePeriod& timePeriod);
    void resetArchiveReaderUnsafe(
        const std::chrono::milliseconds& startTime,
        const std::chrono::milliseconds& endTime);

    void resetRecorderUnsafe(
        const std::chrono::milliseconds& startTime,
        const std::chrono::milliseconds& endTime);

    static QnTimePeriodList toTimePeriodList(
        const std::vector<RemoteArchiveChunk>& chunks);

    static std::chrono::milliseconds totalDuration(const QnTimePeriodList& deviceChunks);

    bool needToReportProgress() const;

    bool saveMotion(const QnConstMetaDataV1Ptr& motion);

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
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

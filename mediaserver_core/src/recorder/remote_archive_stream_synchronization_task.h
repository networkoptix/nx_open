#pragma once

#include <core/resource/abstract_remote_archive_manager.h>
#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <recorder/server_edge_stream_recorder.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/abstract_archive_delegate.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

class RemoteArchiveStreamSynchronizationTask:
    public AbstractRemoteArchiveSynchronizationTask
{
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;
    using RemoteArchiveChunk = nx::core::resource::RemoteArchiveChunk;
    using BufferType = QByteArray;

public:
    RemoteArchiveStreamSynchronizationTask(QnCommonModule* commonModule);

    virtual void setResource(const QnSecurityCamResourcePtr& resource) override;
    virtual void setDoneHandler(std::function<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;

private:
    bool synchronizeArchive();
    bool writeTimePeriodToArchive(const QnTimePeriod& timePeriod);
    void resetArchiveReaderUnsafe(int64_t startTimeMs, int64_t endTimeMs);
    void resetRecorderUnsafe(int64_t startTimeMs, int64_t endTimeMs);

    QnTimePeriodList toTimePeriodList(
        const std::vector<RemoteArchiveChunk>& entries) const;

    std::chrono::milliseconds totalDuration(const QnTimePeriodList& deviceChunks);

    bool needToFireProgress() const;

private:
    mutable QnMutex m_mutex;
    QnCommonModule* m_commonModule;
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

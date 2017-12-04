#pragma once

#include <chrono>
#include <atomic>

#include <boost/optional.hpp>

#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource/security_cam_resource.h>

#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <recorder/server_edge_stream_recorder.h>

#include <nx/streaming/archive_stream_reader.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

class BaseRemoteArchiveSynchronizationTask:
    public AbstractRemoteArchiveSynchronizationTask
{

public:
    BaseRemoteArchiveSynchronizationTask(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& resource);

    virtual void setDoneHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;
    virtual QnUuid id() const override;

protected:
    virtual bool needToReportProgress() const;

    virtual bool prepareDataSource(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk);

    virtual void createArchiveReaderThreadUnsafe(const QnTimePeriod& timePeriod) = 0;

    virtual void createStreamRecorderThreadUnsafe(const QnTimePeriod& timePeriod);

    virtual boost::optional<nx::core::resource::RemoteArchiveChunk> remoteArchiveChunkByTimePeriod(
        const QnTimePeriod& timePeriod) const;

private:
    bool synchronizeArchive();
    bool saveMotion(const QnConstMetaDataV1Ptr& motion);
    bool fetchChunks(std::vector<nx::core::resource::RemoteArchiveChunk>* outChunks);
    bool writeAllTimePeriods(const QnTimePeriodList& timePeriods);
    bool writeTimePeriodToArchive(
        const QnTimePeriod& timePeriod,
        const nx::core::resource::RemoteArchiveChunk& chunk);

    void onFileHasBeenWritten(const std::chrono::milliseconds& fileDuration);
    void onEndOfRecording();

protected:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_wait;

    QnSecurityCamResourcePtr m_resource;
    nx::utils::MoveOnlyFunc<void()> m_doneHandler;

    std::atomic<bool> m_canceled{false};
    std::chrono::milliseconds m_totalDuration{0};
    std::chrono::milliseconds m_importedDuration{0};
    double m_progress = 0;

    std::vector<nx::core::resource::RemoteArchiveChunk> m_chunks;

    std::unique_ptr<QnAbstractArchiveStreamReader> m_archiveReader;
    std::unique_ptr<QnServerEdgeStreamRecorder> m_recorder;

    nx::core::resource::RemoteArchiveSynchronizationSettings m_settings;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

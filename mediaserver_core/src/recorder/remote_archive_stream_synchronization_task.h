#pragma once

#include <core/resource/abstract_remote_archive_manager.h>
#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <nx/streaming/archive_stream_reader.h>


namespace nx {
namespace mediaserver_core {
namespace recorder {

class RemoteArchiveStreamSynchronizationTask:
    public AbstractRemoteArchiveSynchronizationTask
{
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;
    using RemoteArchiveEntry = nx::core::resource::RemoteArchiveChunk;
    using BufferType = QByteArray;

public:
    RemoteArchiveStreamSynchronizationTask(QnCommonModule* commonModule);

    virtual void setResource(const QnSecurityCamResourcePtr& resource) override;
    virtual void setDoneHandler(std::function<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;

private:
    bool synchronizeArchive();
    bool writeEntryToArchive(const RemoteArchiveEntry& entry);
    QnAbstractArchiveStreamReader* archiveReader(int64_t startTimeMs, int64_t endTimeMs);

    QString chunkFileName(const RemoteArchiveEntry& entry) const;

private:
    QnCommonModule* m_commonModule;
    QnSecurityCamResourcePtr m_resource;
    std::atomic<bool> m_canceled;
    std::function<void()> m_doneHandler;
    std::unique_ptr<QnArchiveStreamReader> m_reader;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

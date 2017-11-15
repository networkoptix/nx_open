#pragma once

#include <core/resource/abstract_remote_archive_manager.h>

#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <recorder/remote_archive_synchronizer.h>

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
    using RemoteArchiveEntry = nx::core::resource::RemoteArchiveChunk;
    using BufferType = QByteArray;

public:
    RemoteArchiveSynchronizationTask(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& resource);

    virtual void setDoneHandler(std::function<void()> handler) override;
    virtual void cancel() override;
    virtual bool execute() override;
    virtual QnUuid id() const override;

private:
    bool synchronizeArchive(const QnSecurityCamResourcePtr& resource);

    std::vector<RemoteArchiveEntry> filterEntries(
        const QnSecurityCamResourcePtr& resource,
        const std::vector<RemoteArchiveEntry>& allEntries);

    bool copyEntryToArchive(
        const QnSecurityCamResourcePtr& resource,
        const RemoteArchiveEntry& entry);

    bool writeEntry(
        const QnSecurityCamResourcePtr& resource,
        const RemoteArchiveEntry& entry,
        BufferType* buffer,
        int64_t* outRealDurationMs = nullptr);

    bool convertAndWriteBuffer(
        const QnSecurityCamResourcePtr& resource,
        BufferType* buffer,
        const QString& fileName,
        int64_t startTimeMs,
        const QnResourceAudioLayoutPtr& audioLayout,
        bool needMotion,
        int64_t* outRealDurationMs);

    QnServer::ChunksCatalog chunksCatalogByResolution(const QSize& resolution) const;

    bool isMotionDetectionNeeded(
        const QnSecurityCamResourcePtr& resource,
        QnServer::ChunksCatalog catalog) const;

private:
    QnSecurityCamResourcePtr m_resource;
    std::atomic<bool> m_canceled;
    std::function<void()> m_doneHandler;
    int m_totalChunksToSynchronize;
    int m_currentNumberOfSynchronizedChunks;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

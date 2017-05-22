#pragma once

#include <QtCore/QObject>
#include <QtCore/QByteArray>

#include <set>
#include <map>
#include <vector>
#include <atomic>

#include <core/resource/resource_fwd.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource/resource_media_layout.h>
#include <server/server_globals.h>
#include <utils/common/concurrent.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

class SynchronizationTask
{
    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;
    using RemoteArchiveEntry = nx::core::resource::RemoteArchiveEntry;
    using BufferType = QByteArray;

public:
    SynchronizationTask();
    void setResource(const QnSecurityCamResourcePtr& resource);
    void setDoneHandler(std::function<void()> handler);
    void cancel();

    bool execute();

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

struct SynchronizationTaskContext
{
    std::shared_ptr<SynchronizationTask> task;
    QnConcurrent::QnFuture<void> result;
};


class RemoteArchiveSynchronizer: public QObject
{
    Q_OBJECT

public:
    RemoteArchiveSynchronizer();
    virtual ~RemoteArchiveSynchronizer();

public slots:
    void at_newResourceAdded(const QnResourcePtr& resource);
    void at_resourceInitializationChanged(const QnResourcePtr& resource);
    void at_resourceParentIdChanged(const QnResourcePtr& resource);

private:
    void makeAndRunTaskUnsafe(const QnSecurityCamResourcePtr& resoource);

    void removeTaskFromAwaited(const QnUuid& resource);
    void cancelTaskForResource(const QnUuid& resource);
    void cancelAllTasks();

    void waitForAllTasks();

private:
    mutable QnMutex m_mutex;
    std::set<QnUuid> m_delayedTasks;
    std::map<QnUuid, SynchronizationTaskContext> m_syncTasks;
    std::atomic<bool> m_terminated;
};


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

#include "wearable_upload_manager.h"

#include <QtCore/QFile>
#include <QtCore/QStorageInfo>

#include <recorder/wearable_archive_synchronization_task.h>
#include <recorder/wearable_archive_synchronizer.h>
#include <recorder/storage_manager.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/std/cpp14.h>

#include "media_server_module.h"

using namespace nx::vms::server::recorder;
using namespace nx::vms::common::p2p::downloader;

struct QnWearableUploadManager::Private
{
    QnMutex mutex;
    QHash<QnUuid, WearableArchiveSynchronizationState> stateByCameraId;
};

QnWearableUploadManager::QnWearableUploadManager(QObject* parent):
    nx::vms::server::ServerModuleAware(parent),
    d(new Private())
{
}

QnWearableUploadManager::~QnWearableUploadManager()
{
}

QnWearableStorageStats QnWearableUploadManager::storageStats() const
{
    QnWearableStorageStats result;

    Downloader* downloader = serverModule()->findInstance<Downloader>();
    NX_ASSERT(downloader);

    QStorageInfo info(downloader->downloadsDirectory());

    QString volumeRoot = info.rootPath();
    QnStorageResourcePtr storage = serverModule()->normalStorageManager()->getStorageByVolume(volumeRoot);
    qint64 spaceLimit = storage && storage->isUsedForWriting() ? storage->getSpaceLimit() : 0;

    result.downloaderBytesAvailable = std::max(0ll, info.bytesAvailable() - spaceLimit);
    result.downloaderBytesFree = info.bytesAvailable();

    auto storages = serverModule()->normalStorageManager()->getUsedWritableStorages();
    for (const QnStorageResourcePtr& storage: serverModule()->normalStorageManager()->getUsedWritableStorages())
    {
        qint64 free = storage->getFreeSpace();
        qint64 spaceLimit = storage->getSpaceLimit();
        qint64 available = std::max(0ll, free - spaceLimit);

        result.totalBytesAvailable += available;
    }
    result.haveStorages = !storages.empty();

    return result;
}

bool QnWearableUploadManager::consume(const QnUuid& cameraId, const QnUuid& token, const QString& uploadId, qint64 startTimeMs)
{
    WearableArchiveSynchronizer* synchronizer =
        serverModule()->findInstance<WearableArchiveSynchronizer>();
    NX_ASSERT(synchronizer);

    Downloader* downloader = serverModule()->findInstance<Downloader>();
    NX_ASSERT(downloader);

    std::unique_ptr<QFile> file = std::make_unique<QFile>(downloader->filePath(uploadId));
    if (!file->open(QIODevice::ReadOnly))
        return false;

    QnSecurityCamResourcePtr camera = serverModule()->resourcePool()->
        getResourceById(cameraId).dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return false;

    WearableArchiveTaskPtr task(new WearableArchiveSynchronizationTask(
        serverModule(),
        camera,
        std::move(file),
        startTimeMs));

    QnMutexLocker locker(&d->mutex);
    if (d->stateByCameraId.contains(cameraId))
        return false;
    d->stateByCameraId.insert(cameraId, task->state());
    locker.unlock();

    auto stateChangedHandler =
        [this, cameraId, token, uploadId](const WearableArchiveSynchronizationState& state)
    {
        if (state.status == WearableArchiveSynchronizationState::Finished)
        {
            if (auto downloader = serverModule()->findInstance<Downloader>())
                downloader->deleteFile(uploadId);

            QnMutexLocker locker(&d->mutex);
            d->stateByCameraId.remove(cameraId);
        }
        else
        {
            QnMutexLocker locker(&d->mutex);
            d->stateByCameraId[cameraId] = state;
        }
    };
    connect(task, &WearableArchiveSynchronizationTask::stateChanged, this, stateChangedHandler);

    synchronizer->addTask(camera, task);
    return true;
}

bool QnWearableUploadManager::isConsuming(const QnUuid& cameraId) const
{
    QnMutexLocker locker(&d->mutex);
    return d->stateByCameraId.contains(cameraId);
}

WearableArchiveSynchronizationState QnWearableUploadManager::state(const QnUuid& cameraId) const
{
    QnMutexLocker locker(&d->mutex);
    return d->stateByCameraId.value(cameraId);
}

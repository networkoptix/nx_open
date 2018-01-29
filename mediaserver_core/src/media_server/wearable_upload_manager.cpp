#include "wearable_upload_manager.h"

#include <QtCore/QFile>

#include <recorder/wearable_archive_synchronization_task.h>
#include <recorder/wearable_archive_synchronizer.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/std/cpp14.h>

#include "media_server_module.h"

using namespace nx::mediaserver_core::recorder;
using namespace nx::vms::common::p2p::downloader;

struct QnWearableUploadManager::Private
{
    QnMutex mutex;
    QHash<QnUuid, WearableArchiveSynchronizationState> stateByCameraId;
};

QnWearableUploadManager::QnWearableUploadManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

QnWearableUploadManager::~QnWearableUploadManager()
{
}

bool QnWearableUploadManager::consume(const QnUuid& cameraId, const QnUuid& token, const QString& uploadId, qint64 startTimeMs)
{
    WearableArchiveSynchronizer* synchronizer =
        qnServerModule->findInstance<WearableArchiveSynchronizer>();
    NX_ASSERT(synchronizer);

    Downloader* downloader = qnServerModule->findInstance<Downloader>();
    NX_ASSERT(downloader);

    std::unique_ptr<QFile> file = std::make_unique<QFile>(downloader->filePath(uploadId));
    if (!file->open(QIODevice::ReadOnly))
        return false;

    QnSecurityCamResourcePtr camera = qnServerModule->commonModule()->resourcePool()->
        getResourceById(cameraId).dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return false;

    {
        QnMutexLocker locker(&d->mutex);

        if (d->stateByCameraId.contains(cameraId))
            return false;

        d->stateByCameraId.insert(cameraId, WearableArchiveSynchronizationState());
    }

    WearableArchiveTaskPtr task(new WearableArchiveSynchronizationTask(
        qnServerModule,
        camera,
        std::move(file),
        startTimeMs));

    auto stateChangedHandler =
        [this, cameraId, token, uploadId](const WearableArchiveSynchronizationState& state)
    {
        if (state.status == WearableArchiveSynchronizationState::Finished)
        {
            if (auto downloader = qnServerModule->findInstance<Downloader>())
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

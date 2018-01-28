#include "wearable_upload_manager.h"

#include <QtCore/QFile>

#include <recorder/wearable_archive_synchronization_task.h>
#include <recorder/wearable_archive_synchronizer.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>

#include "media_server_module.h"

QnWearableUploadManager::QnWearableUploadManager(QObject* parent): base_type(parent)
{
}

QnWearableUploadManager::~QnWearableUploadManager()
{
}

bool QnWearableUploadManager::consume(const QnUuid& cameraId, const QnUuid& token, const QString& uploadId, qint64 startTimeMs)
{
    using namespace nx::mediaserver_core::recorder;
    using namespace nx::vms::common::p2p::downloader;

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
        QnMutexLocker locker(&m_mutex);

        if (m_consuming.contains(cameraId))
            return false;

        m_consuming.insert(cameraId);
    }

    WearableArchiveTaskPtr task(new WearableArchiveSynchronizationTask(
        qnServerModule,
        camera,
        std::move(file),
        startTimeMs));

    auto finishedHandler =
        [this, camera, token, uploadId]()
    {
        if (auto downloader = qnServerModule->findInstance<Downloader>())
            downloader->deleteFile(uploadId);

        QnMutexLocker locker(&m_mutex);
        m_consuming.remove(camera->getId());
    };
    connect(task, &WearableArchiveSynchronizationTask::finished, this, finishedHandler);

    synchronizer->addTask(camera, task);
    return true;
}

bool QnWearableUploadManager::isConsuming(const QnUuid& cameraId)
{
    QnMutexLocker locker(&m_mutex);

    return m_consuming.contains(cameraId);
}

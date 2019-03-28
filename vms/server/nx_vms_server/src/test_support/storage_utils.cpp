#include "storage_utils.h"
#include "mediaserver_launcher.h"

#include <plugins/storage/file_storage/file_storage_resource.h>
#include <recorder/storage_manager.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management//resource_pool.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::test_support {

static void waitForStorageToAppear(MediaServerLauncher* server, const QString& storagePath)
{
    while (true)
    {
        const auto allStorages = server->serverModule()->normalStorageManager()->getStorages();

        const auto testStorageIt = std::find_if(
            allStorages.cbegin(), allStorages.cend(),
            [&storagePath](const auto& storage) { return storage->getUrl() == storagePath; });

        if (testStorageIt == allStorages.cend())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        while (true) //< Waiting for initialization
        {
            const auto storage = *testStorageIt;
            const auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
            NX_ASSERT(fileStorage);
            fileStorage->setMounted(true);

            if (!(storage->isWritable() && storage->isOnline() && storage->isUsedForWriting()))
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            else
                break;
        }

        break;
    }
}

void addTestStorage(MediaServerLauncher* server, const QString& storagePath)
{
    NX_ASSERT(!storagePath.isEmpty());

    const auto existingStorages = server->serverModule()->resourcePool()->getResources<QnStorageResource>();
    const auto existingIt = std::find_if(
        existingStorages.cbegin(), existingStorages.cend(),
        [&storagePath](const auto& storage) { return storage->getUrl() == storagePath; });

    if (existingIt != existingStorages.cend())
        return;

    QnStorageResourcePtr storage(server->serverModule()->storagePluginFactory()->createStorage(
            server->commonModule(), "ufile"));

    storage->setName("Test storage");
    storage->setParentId(server->commonModule()->moduleGUID());
    storage->setUrl(storagePath);
    storage->setSpaceLimit(0);
    storage->setStorageType("local");
    storage->setUsedForWriting(storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());

    NX_ASSERT(storage->isUsedForWriting());

    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    if (resType)
        storage->setTypeId(resType->getId());

    QList<QnStorageResourcePtr> storages{ storage };
    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(QList<QnStorageResourcePtr>{storage}, apiStorages);

    const auto ec2Connection = server->serverModule()->ec2Connection();
    const auto saveResult =
        ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages);

    NX_ASSERT(ec2::ErrorCode::ok == saveResult);

    for (const auto &storage : storages)
    {
        server->serverModule()->commonModule()->messageProcessor()->updateResource(
            storage, ec2::NotificationSource::Local);
    }

    server->serverModule()->normalStorageManager()->initDone();
    waitForStorageToAppear(server, storagePath);
}

static ChunkDataList generateChunksForQuality(
    const QString& baseDir, const QString& cameraName, const QString& quality, qint64 startTimeMs,
    int count)
{
    using namespace std::chrono;

    const int durationMs = 70000;
    ChunkDataList result;

    for (int i = 0; i < count; ++i)
    {
        QString fileName = lit("%1_%2.mkv").arg(startTimeMs).arg(durationMs);
        QString pathString = QnStorageManager::dateTimeStr(
            startTimeMs, currentTimeZone() / 60, "/");

        pathString = lit("%2/%1/%3").arg(cameraName).arg(quality).arg(pathString);
        auto fullDirPath = QDir(baseDir).absoluteFilePath(pathString);
        QString fullFileName =
            closeDirPath(baseDir) + closeDirPath(pathString) + fileName;

        NX_ASSERT(QDir().mkpath(fullDirPath));
        NX_ASSERT(QFile(fullFileName).open(QIODevice::WriteOnly));

        result.push_back(ChunkData{ startTimeMs, durationMs });
        startTimeMs += durationMs + 5;
    }

    return result;
}

Catalog generateCameraArchive(
    const QString& baseDir,
    const QString& cameraName,
    qint64 startTimeMs,
    int count)
{
    Catalog result;
    result.lowQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "low_quality", startTimeMs, count);

    result.highQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "hi_quality", startTimeMs, count);

    return result;
}

} // namespace nx::test_support
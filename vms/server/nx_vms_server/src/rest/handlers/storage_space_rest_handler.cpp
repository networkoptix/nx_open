#include "storage_space_rest_handler.h"

#include <api/model/storage_space_reply.h>

#include <common/common_module.h>

#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>

#include <plugins/plugin_manager.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <plugins/storage/file_storage/file_storage_resource.h>

#include <nx/fusion/serialization/json.h>

#include "recorder/storage_manager.h"
#include "media_server/settings.h"

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <nx/network/http/http_types.h>
#include <rest/server/rest_connection_processor.h>
#include <media_server/media_server_module.h>
#include <nx/utils/log/log.h>

namespace
{
    const QString kFastRequestKey("fast");
}

QnStorageSpaceRestHandler::QnStorageSpaceRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{}

int QnStorageSpaceRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* /*owner*/)
{
    /* Some api calls can take a lot of time, so client can make a fast request for the first time. */
    const bool fastRequest = QnLexical::deserialized(params[kFastRequestKey], false);

    QnStorageSpaceReply reply;

    auto enumerate = [fastRequest, &reply, this](
        const QnStorageResourceList& storages,
        const QSet<QnStorageResourcePtr>& writableStorages)
        {
            for (const auto& storage: storages)
            {
                QnStorageSpaceData data(storage, fastRequest);
                data.url = QnStorageResource::urlWithoutCredentials(data.url);
                if (!fastRequest)
                    data.isWritable = writableStorages.contains(storage);
                data.storageStatus = QnStorageManager::storageStatus(serverModule(), storage);
                reply.storages.push_back(data);
            }
        };

    enumerate(
        serverModule()->normalStorageManager()->getStorages(),
        fastRequest
            ? QSet<QnStorageResourcePtr>()
            : serverModule()->normalStorageManager()->getAllWritableStorages());
    enumerate(
        serverModule()->backupStorageManager()->getStorages(),
        fastRequest
            ? QSet<QnStorageResourcePtr>()
            : serverModule()->backupStorageManager()->getAllWritableStorages());

    if (!fastRequest)
    {
        for (const QnStorageSpaceData& optionalStorage: getOptionalStorages())
            reply.storages.push_back(optionalStorage);
    }

    reply.storageProtocols = getStorageProtocols();
    result.setReply(reply);
    NX_DEBUG(this, "Return %1 storages and %2 protocols%3",
        reply.storages.size(), reply.storageProtocols.size(),
        fastRequest ? " on fast request" : "");

    return nx::network::http::StatusCode::ok;
}

QList<QString> QnStorageSpaceRestHandler::getStorageProtocols() const
{
    QList<QString> result;
    auto pluginManager = serverModule()->pluginManager();
    if (!NX_ASSERT(pluginManager))
        return result;

    for (nx_spl::StorageFactory* const storagePlugin:
        pluginManager->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        result.push_back(storagePlugin->storageType());
        storagePlugin->releaseRef();
    }

    result.push_back("smb");
    return result;
}

QList<QString> QnStorageSpaceRestHandler::getStoragePaths() const
{
    QList<QString> storagePaths;
    for(const QnFileStorageResourcePtr &fileStorage: serverModule()->normalStorageManager()->getStorages().filtered<QnFileStorageResource>())
        storagePaths.push_back(QnStorageResource::toNativeDirPath(fileStorage->getPath()));

    for(const QnFileStorageResourcePtr &fileStorage: serverModule()->backupStorageManager()->getStorages().filtered<QnFileStorageResource>())
        storagePaths.push_back(QnStorageResource::toNativeDirPath(fileStorage->getPath()));

    return storagePaths;
}

static bool storageExists(
    const QnPlatformMonitor::PartitionSpace partition,
    const QList<QString>& existingStoragePaths)
{
    return std::any_of(
        existingStoragePaths.cbegin(), existingStoragePaths.cend(),
        [&partition](const QString &storagePath)
        {
            return closeDirPath(storagePath).startsWith(partition.path);
        });
}

static bool containsRestrictedPaths(const QnPlatformMonitor::PartitionSpace& partition)
{
    return partition.path.indexOf(QnFileStorageResource::tempFolderName()) != -1;
}

QList<QnPlatformMonitor::PartitionSpace> QnStorageSpaceRestHandler::getSuitablePartitions() const
{
    const auto allPartitions = ((QnPlatformMonitor*) serverModule()->platform()->monitor())->totalPartitionSpaceInfo(
        QnPlatformMonitor::LocalDiskPartition
        | QnPlatformMonitor::NetworkPartition
        | QnPlatformMonitor::RemovableDiskPartition);

    const auto storagePaths = getStoragePaths();
    QList<QnPlatformMonitor::PartitionSpace> result;
    std::copy_if(
        allPartitions.cbegin(), allPartitions.cend(), std::back_inserter(result),
        [&storagePaths, this](const auto& p)
        {
            return p.sizeBytes > QnFileStorageResource::calcSpaceLimit(serverModule(), p.type)
                && !storageExists(p, storagePaths)
                && !containsRestrictedPaths(p);
        });

    return result;
}

static QString storageUrl(const QString& partitionPath)
{
    return closeDirPath(partitionPath) + QnAppInfo::mediaFolderName();
}

QnStorageResourceList QnStorageSpaceRestHandler::storageListFrom(
    const QList<QnPlatformMonitor::PartitionSpace>& partitions) const
{
    QnStorageResourceList result;
    std::transform(
        partitions.cbegin(), partitions.cend(), std::back_inserter(result),
        [this](const auto& p)
        {

            const auto url = storageUrl(p.path);
            NX_VERBOSE(this, "Creating a file storage for an optional path %1", url);
            const auto commonModule = serverModule()->commonModule();
            auto storage = QnStorageResourcePtr(
                commonModule->storagePluginFactory()->createStorage(commonModule, url, false))
                .dynamicCast<QnFileStorageResource>();

            if (!storage)
            {
                NX_VERBOSE(this, "Failed to create storage with the optional path %1", url);
                return QnStorageResourcePtr();
            }

            if (storage->initOrUpdate() != Qn::StorageInit_Ok)
            {
                NX_VERBOSE(this, "InitOrUpdate failed for storage with the optional path %1", url);
                return QnStorageResourcePtr();
            }

            storage->setStatus(Qn::Online);
            storage->setSpaceLimit(storage->calcInitialSpaceLimit());
            storage->setUrl(url);
            if (!wouldBeWritableInPool(storage))
            {
                NX_VERBOSE(this, "Storage %1 would not be writable if put among other storages", url);
                return QnStorageResourcePtr();
            }

            NX_VERBOSE(this, "Optional storage %1 is operational", url);
            return storage.dynamicCast<QnStorageResource>();
        });


    result.erase(
        std::remove_if(result.begin(), result.end(), [](const auto& s) { return !s; }),
        result.end());

    return result;
}

static QList<QnStorageSpaceData> spaceDataListFrom(const QnStorageResourceList& storages)
{
    QList<QnStorageSpaceData> result;
    std::transform(
        storages.cbegin(), storages.cend(), std::back_inserter(result),
        [](const auto& storage) { return QnStorageSpaceData(storage, /*fastCreate*/ false); });
    return result;
}

bool QnStorageSpaceRestHandler::wouldBeWritableInPool(const QnStorageResourcePtr& storage) const
{
    QnStorageResourceList additionalStorages{storage};
    return serverModule()->normalStorageManager()->getAllWritableStorages(
        &additionalStorages).contains(storage);
}

QnStorageSpaceDataList QnStorageSpaceRestHandler::getOptionalStorages() const
{
    const auto partitions = getSuitablePartitions();
    const auto storages = storageListFrom(partitions);
    return spaceDataListFrom(storages);
}

#include "storage_space_rest_handler.h"

#include <api/model/storage_space_reply.h>

#include <common/common_module.h>

#include <core/resource/storage_resource.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>

#include <platform/platform_abstraction.h>
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
    const QnRestConnectionProcessor* owner)
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
        for (const QnStorageSpaceData& optionalStorage: getOptionalStorages(owner->commonModule()))
            reply.storages.push_back(optionalStorage);
    }

    reply.storageProtocols = getStorageProtocols();
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

QList<QString> QnStorageSpaceRestHandler::getStorageProtocols() const
{
    QList<QString> result;
    auto pluginManager = serverModule()->pluginManager();
    NX_ASSERT(pluginManager, "There should be common module.");
    if (!pluginManager)
        return result;
    for (nx_spl::StorageFactory* const storagePlugin:
        pluginManager->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        result.push_back(storagePlugin->storageType());
        storagePlugin->releaseRef();
    }
    result.push_back(lit("smb"));
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

QnStorageSpaceDataList QnStorageSpaceRestHandler::getOptionalStorages(QnCommonModule* commonModule) const
{
    QnStorageSpaceDataList result;

    auto partitionEnoughSpace = [this](QnPlatformMonitor::PartitionType ptype, qint64 size)
    {
        if (size == QnStorageResource::kUnknownSize)
            return true;
        return size >= QnFileStorageResource(serverModule()).calcSpaceLimit(ptype);
    };

    /* Enumerate auto-generated storages on all possible partitions. */
    QnPlatformMonitor* monitor = serverModule()->platform()->monitor();
    QList<QnPlatformMonitor::PartitionSpace> partitions =
        monitor->totalPartitionSpaceInfo(
            QnPlatformMonitor::LocalDiskPartition
            | QnPlatformMonitor::NetworkPartition
            | QnPlatformMonitor::RemovableDiskPartition);

    for(int i = 0; i < partitions.size(); i++)
        partitions[i].path = QnStorageResource::toNativeDirPath(partitions[i].path);

    const QList<QString> storagePaths = getStoragePaths();
    for(const QnPlatformMonitor::PartitionSpace &partition: partitions)
    {
        if (partition.path.indexOf(NX_TEMP_FOLDER_NAME) != -1)
            continue;

        if (!partitionEnoughSpace(partition.type, partition.sizeBytes))
            continue;

        bool hasStorage = std::any_of(
            storagePaths.cbegin(),
            storagePaths.cend(),
            [&partition](const QString &storagePath)
            {
                return closeDirPath(storagePath).startsWith(partition.path);
            });

        if(hasStorage)
            continue;

        QnStorageSpaceData data;
        data.url = partition.path + QnAppInfo::mediaFolderName();
        data.totalSpace = partition.sizeBytes;
        data.freeSpace = partition.freeBytes;
        data.reservedSpace = QnFileStorageResource(serverModule()).calcSpaceLimit(partition.type);
        data.isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
        data.storageType = QnLexical::serialized(partition.type);

        QnStorageResourcePtr storage = QnStorageResourcePtr(
            QnStoragePluginFactory::instance()->createStorage(commonModule, data.url, false));

        if (storage)
        {
            storage->setUrl(data.url);
            if (storage->getStorageType().isEmpty())
                storage->setStorageType(data.storageType);

            data.storageStatus = QnStorageManager::storageStatus(serverModule(), storage);
            data.isOnline = storage->initOrUpdate() == Qn::StorageInit_Ok;
            if (data.isOnline)
            {
                if (storage->getId().isNull())
                    storage->setId(QnUuid::createUuid());
                storage->setStatus(Qn::Online);
                if (auto fileStorage = storage.dynamicCast<QnFileStorageResource>())
                    storage->setSpaceLimit(fileStorage->calcInitialSpaceLimit());
            }

            QnStorageResourceList additionalStorages;
            additionalStorages.append(storage);
            auto writableStoragesIfCurrentWasAdded = serverModule()->normalStorageManager()->getAllWritableStorages(
                &additionalStorages);

            bool wouldBeWritableIfAmongstServerStorages =
                writableStoragesIfCurrentWasAdded.contains(storage);
            data.isWritable = data.isOnline
                && storage->isWritable()
                && wouldBeWritableIfAmongstServerStorages;

            NX_VERBOSE(
                this,
                lm("[ApiStorageSpace] Optional storage %1, online: %2, isWritable: %3, wouldBeWritableIfAmongstServerStorages: %4")
                    .args(storage->getUrl(), data.isOnline, data.isWritable,
                        wouldBeWritableIfAmongstServerStorages));

            auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
            if (fileStorage && data.isOnline)
                data.reservedSpace = fileStorage->calcInitialSpaceLimit();
        }

        result.push_back(data);
    }

    return result;
}

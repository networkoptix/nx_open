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
#include "../helpers/storage_space_helper.h"

namespace {

static const QString kFastRequestKey("fast");

} // namespace

const QString QnStorageSpaceRestHandler::kOwndedOnlyKey("ownedOnly");

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
    reply.storages = nx::rest::helpers::availableStorages(serverModule());

    if (!fastRequest && !params.contains(kOwndedOnlyKey))
    {
        for (const QnStorageSpaceData& optionalStorage: getOptionalStorages(owner->commonModule()))
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

QnStorageSpaceDataList QnStorageSpaceRestHandler::getOptionalStorages(
    QnCommonModule* commonModule) const
{
    QnStorageSpaceDataList result;

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
        if (partition.path.indexOf(QnFileStorageResource::tempFolderName()) != -1)
        {
            NX_VERBOSE(this, "Ignore temporary optional partition %1", partition);
            continue;
        }

        bool hasStorage = std::any_of(
            storagePaths.cbegin(), storagePaths.cend(),
            [&partition](const QString &storagePath)
            {
                return closeDirPath(storagePath).startsWith(partition.path);
            });

        if (hasStorage)
        {
            NX_VERBOSE(this, "Ignore known optional partition %1", partition);
            continue;
        }

        QnStorageSpaceData data;
        data.url = partition.path + QnAppInfo::mediaFolderName();
        data.totalSpace = partition.sizeBytes;
        data.freeSpace = partition.freeBytes;
        data.isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
        data.storageType = QnLexical::serialized(partition.type);

        auto storage = QnStorageResourcePtr(
            commonModule->storagePluginFactory()->createStorage(
                commonModule,
                data.url,
                false)).dynamicCast<QnFileStorageResource>();

        if (!storage)
        {
            NX_WARNING(this, "Failed to create a storage for the path %1", data.url);
            continue;
        }

        if (!QnStorageManager::canStorageBeUsedByVms(storage))
        {
            NX_DEBUG(
                this, "Storage for the path %1 is too small. Won't be added in the result",
                data.url);
            continue;
        }

        data.reservedSpace = storage->getSpaceLimit();

        storage->setUrl(data.url);
        if (storage->getStorageType().isEmpty())
            storage->setStorageType(data.storageType);

        data.storageStatus = QnStorageManager::storageStatus(serverModule(), storage);
        data.isOnline = storage->initOrUpdate() == Qn::StorageInit_Ok;
        if (data.isOnline)
        {
            if (storage->getId().isNull())
                storage->setIdUnsafe(QnUuid::createUuid());
            storage->setStatus(Qn::Online);
            if (auto fileStorage = storage.dynamicCast<QnFileStorageResource>())
                storage->setSpaceLimit(fileStorage->calcInitialSpaceLimit());
        }

        auto writableStoragesIfCurrentWasAdded =
            serverModule()->normalStorageManager()->getAllWritableStorages({storage});

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

        result.push_back(data);
    }

    return result;
}

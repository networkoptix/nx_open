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
#include "../helpers/storage_space_helper.h"

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
    const bool fastRequest = QnLexical::deserialized(params["fast"], false);

    QnStorageSpaceReply reply;
    reply.storages = nx::rest::helpers::availableStorages(serverModule());

    if (!fastRequest && !params.contains("ownedOnly"))
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
    const nx::vms::server::PlatformMonitor::PartitionSpace partition,
    const QList<QString>& existingStoragePaths)
{
    return std::any_of(
        existingStoragePaths.cbegin(), existingStoragePaths.cend(),
        [&partition](const QString &storagePath)
        {
            return closeDirPath(storagePath).startsWith(partition.path);
        });
}

static bool containsRestrictedPaths(const nx::vms::server::PlatformMonitor::PartitionSpace& partition)
{
    return partition.path.indexOf(QnFileStorageResource::tempFolderName()) != -1;
}

QList<nx::vms::server::PlatformMonitor::PartitionSpace> QnStorageSpaceRestHandler::getSuitablePartitions() const
{
    const auto allPartitions = ((nx::vms::server::PlatformMonitor*) serverModule()->platform()->monitor())->totalPartitionSpaceInfo(
        nx::vms::server::PlatformMonitor::LocalDiskPartition
        | nx::vms::server::PlatformMonitor::NetworkPartition
        | nx::vms::server::PlatformMonitor::RemovableDiskPartition);

    const auto storagePaths = getStoragePaths();
    QList<nx::vms::server::PlatformMonitor::PartitionSpace> result;
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

QnStorageSpaceDataList QnStorageSpaceRestHandler::getOptionalStorages() const
{
    QList<QnStorageSpaceData> result;
    for (const auto& p: getSuitablePartitions())
    {
        const auto url = closeDirPath(p.path) + QnAppInfo::mediaFolderName();
        NX_VERBOSE(this, "Creating a file storage for an optional path %1", url);
        const auto commonModule = serverModule()->commonModule();
        auto storage = QnStorageResourcePtr(
            commonModule->storagePluginFactory()->createStorage(commonModule, url, false))
            .dynamicCast<QnFileStorageResource>();

        if (!storage)
        {
            NX_VERBOSE(this, "Failed to create storage with the optional path %1", url);
            continue;
        }

        storage->setUrl(url);
        if (storage->getStorageType().isNull())
            storage->setStorageType(QnLexical::serialized(p.type));

        NX_VERBOSE(this, "Starting initOrUpdate for storage %1", storage->getUrl());
        if (storage->initOrUpdate() != Qn::StorageInit_Ok)
        {
            NX_VERBOSE(this, "InitOrUpdate failed for storage with the optional path %1", url);
            continue;
        }

        storage->setIdUnsafe(QnUuid::createUuid());
        storage->setStatus(Qn::Online);
        storage->setSpaceLimit(storage->calcInitialSpaceLimit());

        const bool wouldBeWritable = serverModule()->normalStorageManager()->getAllWritableStorages(
            { storage }).contains(storage);

        if (!wouldBeWritable)
            NX_VERBOSE(this, "Storage %1 would not be writable if put among other storages", url);
        else
            NX_VERBOSE(this, "Optional storage %1 is operational", url);

        auto spaceData = QnStorageSpaceData(storage, /*fastCreate*/ false);
        spaceData.storageId = QnUuid(); //< This is needed for client to correctly treat this storage as a new one.
        spaceData.storageStatus = QnStorageManager::storageStatus(serverModule(), storage);
        spaceData.isWritable &= wouldBeWritable;
        result.append(spaceData);
    }

    return result;
}

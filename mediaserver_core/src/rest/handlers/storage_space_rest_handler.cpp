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

#include <utils/serialization/json.h>

#include "recorder/storage_manager.h"
#include "media_server/settings.h"

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <utils/network/http/httptypes.h>

namespace {

#ifdef _DEBUG
    void validateStoragesPool() {
        /* Validate that all storages are placed in the resource pool. */
        QList<QnUuid> managedStorages;
        for (const QnStorageResourcePtr &storage: qnNormalStorageMan->getStorages())
            managedStorages.append(storage->getId());
        for (const QnStorageResourcePtr &storage: qnBackupStorageMan->getStorages())
            managedStorages.append(storage->getId());

        QList<QnUuid> pooledStorages;
        for (const QnStorageResourcePtr &storage: qnResPool->getResourcesByParentId(qnCommon->remoteGUID()).filtered<QnStorageResource>())
            pooledStorages.append(storage->getId());

        Q_ASSERT_X(managedStorages.size() == pooledStorages.size(), Q_FUNC_INFO, "Make sure all storages are in the resource pool");
        Q_ASSERT_X(managedStorages.toSet() == pooledStorages.toSet(), Q_FUNC_INFO, "Make sure all storages are in the resource pool");
    }
#endif

}

QnStorageSpaceRestHandler::QnStorageSpaceRestHandler():
    m_monitor(qnPlatform->monitor()) 
{}

int QnStorageSpaceRestHandler::executeGet(const QString &, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QnStorageSpaceReply reply;

    const qint64 defaultStorageSpaceLimit = MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong();
    auto enoughSpace = [defaultStorageSpaceLimit](qint64 totalSpace) {
        /* We should always display invalid storages. */
        if (totalSpace == QnStorageResource::UnknownSize)
            return true;
        return totalSpace >= defaultStorageSpaceLimit;
    };

    auto filterDeprecated = [] (const QnStorageResourcePtr &storage){
        return !storage->hasFlags(Qn::deprecated);
    };

#ifdef _DEBUG
    validateStoragesPool();
#endif

    /* Enumerate normal storages. */
    for (const QnStorageResourcePtr &storage: qnNormalStorageMan->getStorages().filtered(filterDeprecated)) {      
        QnStorageSpaceData data(storage);
        if (!enoughSpace(data.totalSpace)) 
            data.isWritable = false;
        reply.storages.push_back(data); 
    }

    /* Enumerate backup storages. */
    for (const QnStorageResourcePtr &storage: qnBackupStorageMan->getStorages().filtered(filterDeprecated)) {      
        QnStorageSpaceData data(storage);
            if (!enoughSpace(data.totalSpace)) 
                data.isWritable = false;
        reply.storages.push_back(data); 
    }


    /* Enumerate auto-generated storages on all possible partitions. */
    QList<QnPlatformMonitor::PartitionSpace> partitions =
        m_monitor->totalPartitionSpaceInfo( QnPlatformMonitor::LocalDiskPartition | QnPlatformMonitor::NetworkPartition );

    for(int i = 0; i < partitions.size(); i++)
        partitions[i].path = QnStorageResource::toNativeDirPath(partitions[i].path);

    const QList<QString> storagePaths = getStoragePaths();
    for(const QnPlatformMonitor::PartitionSpace &partition: partitions) {
        if (partition.path.indexOf(NX_TEMP_FOLDER_NAME) != -1)
            continue;

        if (!enoughSpace(partition.sizeBytes))
            continue;

        bool hasStorage = std::any_of(storagePaths.cbegin(), storagePaths.cend(), [&partition](const QString &storagePath) {
            return closeDirPath(storagePath).startsWith(partition.path);
        });

        if(hasStorage)
            continue;

        QnStorageSpaceData data;
        data.url = partition.path + QnAppInfo::mediaFolderName();
        data.totalSpace = partition.sizeBytes;
        data.freeSpace = partition.freeBytes;
        data.reservedSpace = defaultStorageSpaceLimit;
        data.isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
        data.storageType = QnLexical::serialized(partition.type);

        QnStorageResourcePtr storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(data.url, false));
        if (storage) {
            storage->setUrl(data.url); /* createStorage does not fill url. */
            storage->setSpaceLimit(defaultStorageSpaceLimit);
            if (storage->getStorageType().isEmpty())
                storage->setStorageType(data.storageType);
            data.isWritable = storage->isWritable();
        } 

        reply.storages.push_back(data);
    }


    reply.storageProtocols = getStorageProtocols();

    result.setReply(reply);
    return nx_http::StatusCode::ok;
}

QList<QString> QnStorageSpaceRestHandler::getStorageProtocols() const {
    QList<QString> result;
    for (const auto storagePlugin : PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
        result.push_back(storagePlugin->storageType());
    result.push_back(lit("smb"));
    return result;
}

QList<QString> QnStorageSpaceRestHandler::getStoragePaths() const {
    QList<QString> storagePaths;
    for(const QnFileStorageResourcePtr &fileStorage: qnNormalStorageMan->getStorages().filtered<QnFileStorageResource>()) 
        storagePaths.push_back(QnStorageResource::toNativeDirPath(fileStorage->getLocalPath()));

    for(const QnFileStorageResourcePtr &fileStorage: qnBackupStorageMan->getStorages().filtered<QnFileStorageResource>()) 
        storagePaths.push_back(QnStorageResource::toNativeDirPath(fileStorage->getLocalPath()));

    return storagePaths;
}

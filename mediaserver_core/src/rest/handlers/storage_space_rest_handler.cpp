#include "storage_space_rest_handler.h"

#include <api/model/storage_space_reply.h>

#include <core/resource/storage_resource.h>
#include <core/resource/storage_plugin_factory.h>

#include <platform/platform_abstraction.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <plugins/storage/file_storage/file_storage_resource.h>

#include <utils/network/tcp_connection_priv.h> /* For CODE_OK. */
#include <utils/serialization/json.h>

#include "recorder/storage_manager.h"
#include "media_server/settings.h"

#include <utils/common/app_info.h>
#include <utils/common/util.h>

QnStorageSpaceRestHandler::QnStorageSpaceRestHandler():
    m_monitor(qnPlatform->monitor()) 
{}

int QnStorageSpaceRestHandler::executeGet(const QString &, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QnStorageSpaceReply reply;

    bool useMainPool = !params.contains("mainPool") || params.value("mainPool").toInt() != 0;
    bool useBackup = !useMainPool;

    const qint64 defaultStorageSpaceLimit = MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong();
    auto enoughSpace = [defaultStorageSpaceLimit](const QnStorageSpaceData &data) {
        /* We should always display invalid storages. */
        if (data.totalSpace == QnStorageResource::UnknownSize)
            return true;
        return data.totalSpace >= defaultStorageSpaceLimit;
    };

    auto storageMan = useMainPool ? qnNormalStorageMan : qnBackupStorageMan;
    QList<QString> storagePaths;
    for(const QnStorageResourcePtr &storage: storageMan->getStorages()) 
    {
        QString path;
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource>(storage);
        if (fileStorage)
            path = QnStorageResource::toNativeDirPath(fileStorage->getLocalPath());
        
        if (storage->hasFlags(Qn::deprecated))
            continue;

        QnStorageSpaceData data;
        data.url = storage->getUrl();
        data.storageId = storage->getId();
        data.totalSpace = storage->getTotalSpace();
        data.freeSpace = storage->getFreeSpace();
        data.isWritable = storage->getCapabilities() & QnAbstractStorageResource::cap::WriteFile;
        data.reservedSpace = storage->getSpaceLimit();
        data.isExternal = storage->isExternal();
        data.isUsedForWriting = storage->isUsedForWriting();
        data.storageType = storage->getStorageType();

        if (!enoughSpace(data))
            continue;

        reply.storages.push_back(data);
        storagePaths.push_back(path);        
    }

    if (useMainPool)
    {
        for (auto &storage : qnBackupStorageMan->getStorages())
        {
            QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource>(storage);
            if (fileStorage != nullptr)
                storagePaths.push_back(QnStorageResource::toNativeDirPath(fileStorage->getLocalPath()));
        }

        QList<QnPlatformMonitor::PartitionSpace> partitions =
            m_monitor->totalPartitionSpaceInfo(
            QnPlatformMonitor::LocalDiskPartition |
            QnPlatformMonitor::NetworkPartition
            );

        for(int i = 0; i < partitions.size(); i++)
            partitions[i].path = QnStorageResource::toNativeDirPath(partitions[i].path);

        for(const QnPlatformMonitor::PartitionSpace &partition: partitions) 
        {
            if (partition.path.indexOf(NX_TEMP_FOLDER_NAME) != -1)
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

            if (!enoughSpace(data))
                continue;

            QnStorageResourcePtr storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(data.url, false));
            if (storage) {
                storage->setUrl(data.url); /* createStorage does not fill url. */
                storage->setSpaceLimit(defaultStorageSpaceLimit);
                if (storage->getStorageType().isEmpty())
                    storage->setStorageType(data.storageType);
                data.isWritable = storage->getCapabilities() & QnAbstractStorageResource::cap::WriteFile;
            } 

            reply.storages.push_back(data);
        }
    }

    for (const auto storagePlugin : 
         PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        reply.storageProtocols.push_back(storagePlugin->storageType());
    }

    reply.storageProtocols.push_back(lit("smb"));

    result.setReply(reply);
    return CODE_OK;
}

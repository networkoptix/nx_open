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

int QnStorageSpaceRestHandler::executeGet(const QString &, const QnRequestParams &, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QnStorageSpaceReply reply;

    QList<QnPlatformMonitor::PartitionSpace> partitions =
        m_monitor->totalPartitionSpaceInfo(
            QnPlatformMonitor::LocalDiskPartition |
            QnPlatformMonitor::NetworkPartition
        );

    for(int i = 0; i < partitions.size(); i++)
        partitions[i].path = QnStorageResource::toNativeDirPath(partitions[i].path);

    QList<QString> storagePaths;
    for(const QnStorageResourcePtr &storage: qnStorageMan->getStorages()) {
        QString path;
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource>(storage);
        if (fileStorage != nullptr)
            path = QnStorageResource::toNativeDirPath(fileStorage->getLocalPath());
        
        if (storage->hasFlags(Qn::deprecated))
            continue;

        QnStorageSpaceData data;
        data.url = storage->getUrl();
        data.storageId = storage->getId();
        if (storage->getStatus() == Qn::Online) {
            data.totalSpace = storage->getTotalSpace();
            data.freeSpace = storage->getFreeSpace();
            data.isWritable = storage->getCapabilities() & 
                              QnAbstractStorageResource::cap::WriteFile;
        }
        data.reservedSpace = storage->getSpaceLimit();
        data.isExternal = storage->isExternal();
        data.isUsedForWriting = storage->isUsedForWriting();
        data.storageType = storage->getStorageType();

        if( data.totalSpace != -1 && data.totalSpace < MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() )
            continue;

        // TODO: #Elric remove once UnknownSize is dropped.
        if(data.totalSpace == QnStorageResource::UnknownSize)
            data.totalSpace = -1;
        if(data.freeSpace == QnStorageResource::UnknownSize)
            data.freeSpace = -1;

        reply.storages.push_back(data);
        storagePaths.push_back(path);        
    }

    for(const QnPlatformMonitor::PartitionSpace &partition: partitions) {
        bool hasStorage = false;
        for(const QString &storagePath: storagePaths) {
            if (closeDirPath(storagePath).startsWith(partition.path) ||
                partition.path.indexOf(NX_TEMP_FOLDER_NAME) != -1)
            {
                hasStorage = true;
                break;
            }
        }
        if(hasStorage)
            continue;

        const qint64 defaultStorageSpaceLimit = MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong();

        QnStorageSpaceData data;
        data.url = partition.path + QnAppInfo::mediaFolderName();
        data.storageId = QnUuid();
        data.totalSpace = partition.sizeBytes;
        data.freeSpace = partition.freeBytes;
        data.reservedSpace = defaultStorageSpaceLimit;
        data.isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
        data.isUsedForWriting = false;
        data.storageType = QnLexical::serialized(partition.type);

        if( data.totalSpace < defaultStorageSpaceLimit )
            continue;

        QnStorageResourcePtr storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(data.url, false));
        if (storage) {
            storage->setUrl(data.url); /* createStorage does not fill url. */
            storage->setSpaceLimit(defaultStorageSpaceLimit);
            if (storage->getStorageType().isEmpty())
                storage->setStorageType(data.storageType);
            data.isWritable = storage->getCapabilities() & 
                              QnAbstractStorageResource::cap::WriteFile;
        } else {
            data.isWritable = false;
        }

        reply.storages.push_back(data);
    }
    
    for (const auto storagePlugin : 
         PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        reply.storageProtocols.push_back(storagePlugin->storageType());
    }
//#ifdef Q_OS_WIN
    reply.storageProtocols.push_back(lit("smb"));
    /* Coldstore is not supported for now as nobody uses it. */
//#endif

    result.setReply(reply);
    return CODE_OK;
}

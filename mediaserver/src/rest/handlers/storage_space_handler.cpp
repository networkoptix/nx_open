#include "storage_space_handler.h"

#include <QtCore/QDir>

#include <utils/network/tcp_connection_priv.h> /* For CODE_OK. */
#include <utils/common/json.h>

#include <api/model/storage_space_reply.h>

#include <platform/core_platform_abstraction.h>

#include <recorder/storage_manager.h>

#include <version.h>

namespace {
    QString toNativeDirPath(const QString &dirPath) {
        QString result = QDir::toNativeSeparators(dirPath);
        if(!result.endsWith(QDir::separator()))
            result.append(QDir::separator());
        return result;
    }

} // anonymous namespace

QnStorageSpaceRestHandler::QnStorageSpaceRestHandler():
    m_monitor(qnPlatform->monitor()) 
{}

int QnStorageSpaceRestHandler::executeGet(const QString &, const QnRequestParams &, QnJsonRestResult &result) {
    QnStorageSpaceReply reply;

    QList<QnPlatformMonitor::PartitionSpace> partitions = m_monitor->totalPartitionSpaceInfo(QnPlatformMonitor::LocalDiskPartition | QnPlatformMonitor::NetworkPartition);
    for(int i = 0; i < partitions.size(); i++)
        partitions[i].path = toNativeDirPath(partitions[i].path);

    QList<QString> storagePaths;
    foreach(const QnStorageResourcePtr &storage, qnStorageMan->getStorages()) {
        QString path = toNativeDirPath(storage->getUrl());
        
        bool isExternal = true;
        foreach(const QnPlatformMonitor::PartitionSpace &partition, partitions) {
            if(path.startsWith(partition.path)) {
                isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
                break;
            }
        }

        if (storage->hasFlags(QnResource::deprecated))
            continue;

        QnStorageSpaceData data;
        data.path = storage->getUrl();
        data.storageId = storage->getId();
        data.totalSpace = storage->getTotalSpace();
        data.freeSpace = storage->getFreeSpace();
        data.reservedSpace = storage->getSpaceLimit();
        data.isExternal = isExternal;
        data.isWritable = storage->isStorageAvailableForWriting();
        data.isUsedForWriting = storage->isUsedForWriting();

        if( data.totalSpace < QnStorageManager::DEFAULT_SPACE_LIMIT )
            continue;

        // TODO: #Elric remove once UnknownSize is dropped.
        if(data.totalSpace == QnStorageResource::UnknownSize)
            data.totalSpace = -1;
        if(data.freeSpace == QnStorageResource::UnknownSize)
            data.freeSpace = -1;

        reply.storages.push_back(data);
        storagePaths.push_back(path);
    }

    foreach(const QnPlatformMonitor::PartitionSpace &partition, partitions) {
        bool hasStorage = false;
        foreach(const QString &storagePath, storagePaths) {
            if(storagePath.startsWith(partition.path)) {
                hasStorage = true;
                break;
            }
        }
        if(hasStorage)
            continue;

        QnStorageSpaceData data;
        data.path = partition.path + lit(QN_MEDIA_FOLDER_NAME) + QDir::separator();
        data.storageId = -1;
        data.totalSpace = partition.sizeBytes;
        data.freeSpace = partition.freeBytes;
        data.reservedSpace = -1;
        data.isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
        data.isUsedForWriting = false;

        if( data.totalSpace < QnStorageManager::DEFAULT_SPACE_LIMIT )
            continue;

        QnStorageResourcePtr storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(data.path, false));
        if (storage) {
            storage->setUrl(data.path); /* createStorage does not fill url. */
            data.isWritable = storage->isStorageAvailableForWriting();
        } else {
            data.isWritable = false;
        }

        reply.storages.push_back(data);
    }

#ifdef Q_OS_WIN
    reply.storageProtocols.push_back(lit("smb"));
    /* Coldstore is not supported for now as nobody uses it. */
#endif

    result.setReply(reply);
    return CODE_OK;
}

QString QnStorageSpaceRestHandler::description() const {
    return 
        "Returns a list of all server storages.<br>"
        "No parameters.<br>";
}

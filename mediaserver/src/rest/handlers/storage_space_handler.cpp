#include "storage_space_handler.h"

#include <QtCore/QDir>

#include <utils/network/tcp_connection_priv.h> /* For CODE_OK. */
#include <utils/common/json.h>

#include <api/model/storage_space_reply.h>

#include <platform/platform_abstraction.h>

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

QnStorageSpaceHandler::QnStorageSpaceHandler():
    m_monitor(qnPlatform->monitor()) 
{}

int QnStorageSpaceHandler::executeGet(const QString &, const QnRequestParamList &, JsonResult &result) {
    QnStorageSpaceReply reply;

    QList<QnPlatformMonitor::PartitionSpace> partitions = m_monitor->totalPartitionSpaceInfo();
    for(int i = 0; i < partitions.size(); i++)
        partitions[i].path = toNativeDirPath(partitions[i].path);

    QList<QString> storagePaths;
    foreach(const QnStorageResourcePtr &storage, qnStorageMan->getStorages()) {
        QString path = toNativeDirPath(storage->getUrl());
        
        bool hasPartition = false;
        foreach(const QnPlatformMonitor::PartitionSpace &partition, partitions) {
            if(path.startsWith(partition.path)) {
                hasPartition = true;
                break;
            }
        }

        QnStorageSpaceData data;
        data.path = storage->getUrl();
        data.storageId = storage->getId();
        data.totalSpace = storage->getTotalSpace();
        data.freeSpace = storage->getFreeSpace();
        data.reservedSpace = storage->getSpaceLimit();
        data.isExternal = !hasPartition;
        data.isWritable = storage->isStorageAvailableForWriting();
        data.isUsedForWriting = storage->isUsedForWriting();

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
        data.isExternal = false;
        data.isUsedForWriting = false;

        QnStorageResourcePtr storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(data.path, false));
        if (storage) {
            storage->setUrl(data.path); /* createStorage does not fill url. */
            data.isWritable = storage->isStorageAvailableForWriting();
        } else {
            data.isWritable = false;
        }

        reply.storages.push_back(data);
    }

    // if storage total amount is low, and large storages is presents, remove small storages from the user control
    // (writer omits these storages too)
    if (qnStorageMan->isBigStorageExists()) {
        for (int i = 0; i < reply.storages.size(); ++i) {
            if (reply.storages[i].totalSpace < QnStorageManager::BIG_STORAGE_THRESHOLD) {
                reply.storages[i].isWritable = false;
                reply.storages[i].isUsedForWriting = false;
            }
        }
    }

#ifdef Q_OS_WIN
    reply.storageProtocols.push_back(lit("smb"));
#endif
    // TODO: #Elric check for other plugins, e.g. coldstore

    result.setReply(reply);
    return CODE_OK;
}

QString QnStorageSpaceHandler::description(TCPSocket *) const {
    return QString(); // TODO: #Elric
}

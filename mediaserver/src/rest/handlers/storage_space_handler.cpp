#include "storage_space_handler.h"

#include <QtCore/QDir>

#include <utils/network/tcp_connection_priv.h> /* For CODE_OK. */
#include <utils/common/json.h>

#include <api/model/storage_space_reply.h>

#include <platform/platform_abstraction.h>

#include <recorder/storage_manager.h>

#include <version.h>

QnStorageSpaceHandler::QnStorageSpaceHandler():
    m_monitor(qnPlatform->monitor()) 
{}

int QnStorageSpaceHandler::executeGet(const QString &, const QnRequestParamList &, JsonResult &result) {
    QnStorageSpaceReply reply;

    QList<QString> storagePaths;
    foreach(const QnStorageResourcePtr &storage, qnStorageMan->getStorages()) {
        QnStorageSpaceData data;
        data.path = QDir::toNativeSeparators(storage->getUrl());
        data.storageId = storage->getId();
        data.totalSpace = storage->getTotalSpace();
        data.freeSpace = storage->getFreeSpace();
        data.reservedSpace = storage->getSpaceLimit();
        data.isWritable = storage->isStorageAvailableForWriting();
        data.isUsedForWriting = storage->isUsedForWriting();

        // TODO: #Elric remove once UnknownSize is dropped.
        if(data.totalSpace == QnStorageResource::UnknownSize)
            data.totalSpace = -1;
        if(data.freeSpace == QnStorageResource::UnknownSize)
            data.freeSpace = -1;

        reply.storages.push_back(data);
        storagePaths.push_back(data.path);
    }

    foreach(const QnPlatformMonitor::PartitionSpace &partition, m_monitor->totalPartitionSpaceInfo()) {
        QString path = QDir::toNativeSeparators(partition.path);
        if(!path.endsWith(QDir::separator()))
            path.append(QDir::separator());

        bool hasStorage = false;
        foreach(const QString &storagePath, storagePaths) {
            if(storagePath.startsWith(path)) {
                hasStorage = true;
                break;
            }
        }
        if(hasStorage)
            continue;

        QnStorageSpaceData data;
        data.path = path + lit(QN_MEDIA_FOLDER_NAME);
        data.storageId = -1;
        data.totalSpace = partition.sizeBytes;
        data.freeSpace = partition.freeBytes;
        data.reservedSpace = -1;
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

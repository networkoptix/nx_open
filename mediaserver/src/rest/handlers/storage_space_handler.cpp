#include "storage_space_handler.h"

#include <utils/network/tcp_connection_priv.h> /* For CODE_OK. */
#include <utils/common/json.h>

#include <api/model/storage_space_data.h>

#include <platform/platform_abstraction.h>

#include <recorder/storage_manager.h>

#include <version.h>


QnStorageSpaceHandler::QnStorageSpaceHandler():
    m_monitor(qnPlatform->monitor()) 
{}

int QnStorageSpaceHandler::executeGet(const QString &path, const QnRequestParamList &, QByteArray &result, QByteArray &contentType) {
    QList<QnStorageSpaceData> infos;

    QList<QString> storagePaths;
    foreach(const QnStorageResourcePtr &storage, qnStorageMan->getStorages()) {
        QnStorageSpaceData info;
        info.path = storage->getUrl();
        info.storageId = storage->getId();
        info.totalSpace = storage->getTotalSpace();
        info.freeSpace = storage->getFreeSpace();

        // TODO: #Elric remove once UnknownSize is dropped.
        if(info.totalSpace == QnStorageResource::UnknownSize)
            info.totalSpace = -1;
        if(info.freeSpace == QnStorageResource::UnknownSize)
            info.freeSpace = -1;

        infos.push_back(info);

        storagePaths.push_back(storage->getUrl());
    }

    foreach(const QnPlatformMonitor::PartitionSpace &partition, m_monitor->totalPartitionSpaceInfo()) {
        bool hasStorage = false;
        foreach(const QString &storagePath, storagePaths) {
            if(storagePath.startsWith(partition.path)) {
                hasStorage = true;
                break;
            }
        }
        if(hasStorage)
            continue;

        QnStorageSpaceData info;
        info.path = partition.path + lit(QN_MEDIA_FOLDER_NAME);
        info.storageId = -1;
        info.totalSpace = partition.sizeBytes;
        info.freeSpace = partition.freeBytes;
        infos.push_back(info);
    }

    QVariantMap root;
    QJson::serialize(infos, "data", &root);
    QJson::serialize(root, &result);
    contentType = "application/json";

    return CODE_OK;
}

int QnStorageSpaceHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &, QByteArray &result, QByteArray &contentType) {
    return executeGet(path, params, result, contentType);
}

QString QnStorageSpaceHandler::description(TCPSocket *tcpSocket) const {
    return QString();
}

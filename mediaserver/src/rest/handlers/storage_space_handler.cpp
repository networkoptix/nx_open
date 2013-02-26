#include "storage_space_handler.h"

#include <QtCore/QDir>

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
    QnStorageSpaceDataList infos;

    QList<QString> storagePaths;
    foreach(const QnStorageResourcePtr &storage, qnStorageMan->getStorages()) {
        QnStorageSpaceData info;
        info.path = QDir::toNativeSeparators(storage->getUrl());
        info.storageId = storage->getId();
        info.totalSpace = storage->getTotalSpace();
        info.freeSpace = storage->getFreeSpace();

        // TODO: #Elric remove once UnknownSize is dropped.
        if(info.totalSpace == QnStorageResource::UnknownSize)
            info.totalSpace = -1;
        if(info.freeSpace == QnStorageResource::UnknownSize)
            info.freeSpace = -1;

        infos.push_back(info);
        storagePaths.push_back(info.path);
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

        QnStorageSpaceData info;
        info.path = path + lit(QN_MEDIA_FOLDER_NAME);
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

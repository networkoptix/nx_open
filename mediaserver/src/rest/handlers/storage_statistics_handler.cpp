#include "storage_statistics_handler.h"

#include <utils/network/tcp_connection_priv.h> /* For CODE_OK. */
#include <utils/common/json.h>

#include <platform/platform_abstraction.h>

#include <recorder/storage_manager.h>

// TODO: move out
struct QnStorageSpaceData {
    QString path;
    int storageId;
    qint64 totalSpace;
    qint64 freeSpace;
};

void serialize(const QnStorageSpaceData &value, QVariant *target) {
    QVariantMap result;
    QJson::serialize(value.path, "path", &result);
    QJson::serialize(value.storageId, "storageId", &result);
    QJson::serialize(value.totalSpace, "totalSpace", &result);
    QJson::serialize(value.freeSpace, "freeSpace", &result);
    *target = result;
}

bool deserialize(const QVariant &value, QnStorageSpaceData *target) {
    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    QnStorageSpaceData result;
    if(
        !QJson::deserialize(map, "path", &result.path) || 
        !QJson::deserialize(map, "storageId", &result.storageId) ||
        !QJson::deserialize(map, "totalSpace", &result.totalSpace) || 
        !QJson::deserialize(map, "freeSpace", &result.freeSpace)
    ) {
        return false;
    }

    *target = result;
    return true;
}


QnStorageStatisticsHandler::QnStorageStatisticsHandler():
    m_monitor(qnPlatform->monitor()) 
{}

int QnStorageStatisticsHandler::executeGet(const QString &path, const QnRequestParamList &, QByteArray &result, QByteArray &contentType) {
    QList<QnStorageSpaceData> infos;

    QList<QString> storagePaths;
    foreach(const QnStorageResourcePtr &storage, qnStorageMan->getAllStorages()) {
        QnStorageSpaceData info;
        info.path = storage->getUrl();
        info.storageId = storage->getId();
        info.totalSpace = storage->getTotalSpace(); // TODO: wrong results for coldstore.
        info.freeSpace = storage->getFreeSpace();
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
        info.path = partition.path + lit("Media"); // TODO: customization-based name
        info.storageId = -1;
        info.totalSpace = partition.sizeBytes;
        info.freeSpace = partition.freeBytes;
        infos.push_back(info);
    }

    QVariantMap root;
    QJson::serialize(infos, "storages", &root);
    QJson::serialize(root, &result);
    contentType = "application/json";

    return CODE_OK;
}

int QnStorageStatisticsHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &, QByteArray &result, QByteArray &contentType) {
    return executeGet(path, params, result, contentType);
}

QString QnStorageStatisticsHandler::description(TCPSocket *tcpSocket) const {
    return QString();
}

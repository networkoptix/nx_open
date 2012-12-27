#include "file_system_handler.h"

#include <QtCore/QFileInfo>

#include <api/serializer/serializer.h>
#include <core/resource_managment/resource_pool.h>
#include <platform/platform_abstraction.h>
#include <qjson/serializer.h>
#include <recorder/storage_manager.h>
#include <rest/server/rest_server.h>
#include <utils/common/util.h>
#include <utils/network/tcp_connection_priv.h>

QnFileSystemHandler::QnFileSystemHandler() {
    m_monitor = qnPlatform->monitor();
}

int QnFileSystemHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    contentType = "application/json";

    const QnStorageManager::StorageMap storageMap = qnStorageMan->getAllStorages();

    QVariantList partitions;

    foreach(const QnPlatformMonitor::PartitionSpace &space, m_monitor->totalPartitionSpaceInfo()) {
        QVariantMap partition;

        partition.insert("url", space.partition);
        partition.insert("free", space.freeBytes);
        partition.insert("size", space.sizeBytes);

        {   //storages at this partition
            QVariantList storages;
            for(QnStorageManager::StorageMap::const_iterator itr = storageMap.constBegin(); itr != storageMap.constEnd(); ++itr)
            {
                QString root = itr.value()->getUrl();
                if (m_monitor->partitionByPath(root) == space.partition) {
                    QVariantMap storage;
                    storage.insert("url", root);
                    storage.insert("used", itr.value()->getWritedSpace());
                    storages << storage;
                }
            }
            partition.insert("storages", storages);
        }

        partitions << partition;
    }
    QJson::Serializer json;
    result.append(json.serialize(partitions));

    return CODE_OK;
}

int QnFileSystemHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnFileSystemHandler::description() const
{
    QString rez;
    rez += "Returns storage free space and current usage in bytes. if specified folder can not be used for writing or not available returns -1.\n";
    rez += "<BR>Param <b>path</b> - Folder.";
    rez += "<BR><b>Return</b> XML - free space in bytes or -1";
    return rez;
}

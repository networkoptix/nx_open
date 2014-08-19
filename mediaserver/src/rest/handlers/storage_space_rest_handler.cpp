#include "storage_space_rest_handler.h"

#include <QtCore/QDir>

#include <api/model/storage_space_reply.h>

#include <core/resource/storage_resource.h>

#include <platform/platform_abstraction.h>

#include <utils/network/tcp_connection_priv.h> /* For CODE_OK. */
#include <utils/serialization/json.h>

#include "recorder/storage_manager.h"
#include "media_server/settings.h"

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
        QString path = toNativeDirPath(storage->getPath());
        
        bool isExternal = true;
        foreach(const QnPlatformMonitor::PartitionSpace &partition, partitions) {
            if(path.startsWith(partition.path)) {
                isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
                break;
            }
        }

        if (storage->hasFlags(Qn::deprecated))
            continue;

        QnStorageSpaceData data;
        data.url = storage->getUrl();
        data.storageId = storage->getId();
        data.totalSpace = storage->getTotalSpace();
        data.freeSpace = storage->getFreeSpace();
        data.reservedSpace = storage->getSpaceLimit();
        data.isExternal = isExternal;
        data.isWritable = storage->isStorageAvailableForWriting();
        data.isUsedForWriting = storage->isUsedForWriting();

        if( data.totalSpace < MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() )
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

        const qint64 defaultStorageSpaceLimit = MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong();

        QnStorageSpaceData data;
        data.url = partition.path + lit(QN_MEDIA_FOLDER_NAME);
        data.storageId = QUuid();
        data.totalSpace = partition.sizeBytes;
        data.freeSpace = partition.freeBytes;
        data.reservedSpace = defaultStorageSpaceLimit;
        data.isExternal = partition.type == QnPlatformMonitor::NetworkPartition;
        data.isUsedForWriting = false;

        if( data.totalSpace < defaultStorageSpaceLimit )
            continue;

        QnStorageResourcePtr storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(data.url, false));
        if (storage) {
            storage->setUrl(data.url); /* createStorage does not fill url. */
            storage->setSpaceLimit( defaultStorageSpaceLimit );
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

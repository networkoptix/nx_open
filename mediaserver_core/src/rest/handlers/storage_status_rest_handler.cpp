#include "storage_status_rest_handler.h"

#include <QtCore/QFileInfo>

#include "utils/network/tcp_connection_priv.h"

#include "core/resource_management/resource_pool.h"
#include <core/resource/storage_resource.h>
#include <core/resource/storage_plugin_factory.h>

#include <platform/platform_abstraction.h>

#include "utils/common/util.h"
#include "recorder/storage_manager.h"
#include "api/model/storage_status_reply.h"
#include "media_server/settings.h"


int QnStorageStatusRestHandler::executeGet(const QString &, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    QString storageUrl;
    if(!requireParameter(params, lit("path"), result, &storageUrl))
        return CODE_INVALID_PARAMETER;

    QnStorageResourcePtr storage = qnNormalStorageMan->getStorageByUrlExact(storageUrl);
    if (!storage)
        storage = qnBackupStorageMan->getStorageByUrlExact(storageUrl);
        
    if (!storage) {
        storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(storageUrl, false));
        if(storage) {
            storage->setUrl(storageUrl);
            storage->setSpaceLimit(nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE);

            //const auto storagePath = QnStorageResource::toNativeDirPath(storage->getUrl());
            //const auto partitions = qnPlatform->monitor()->totalPartitionSpaceInfo();
            //const auto it = std::find_if(partitions.begin(), partitions.end(),
            //                             [&](const QnPlatformMonitor::PartitionSpace& part)
            //    { return storagePath.startsWith(QnStorageResource::toNativeDirPath(part.path)); });
    
            //const auto storageType = (it != partitions.end()) ? it->type : QnPlatformMonitor::NetworkPartition;
            //if (storage->getStorageType().isEmpty())
            //    storage->setStorageType(QnLexical::serialized(storageType));
        }
        else
            return CODE_INVALID_PARAMETER;
    }
    
    bool exists = !storage.isNull();

    QnStorageStatusReply reply;
    reply.pluginExists = exists;
    reply.storage.url           = storageUrl;

    if (storage) {
        reply.storage.storageId     = storage->getId();
        reply.storage.freeSpace     = storage->getFreeSpace();
        reply.storage.reservedSpace = storage->getSpaceLimit();
        reply.storage.totalSpace    = storage->getTotalSpace();
        reply.storage.isExternal    = storage->isExternal();
        reply.storage.storageType   = storage->getStorageType();

#ifdef WIN32
        if (!reply.storage.isExternal) {
            //TODO: #rvasilenko this 'if' looks strange. BTW, this handler is not used in client
            reply.storage.isWritable = false;
            reply.storage.isUsedForWriting = false;
        }
        else 
#endif
        {
            reply.storage.isWritable = storage->getCapabilities() & QnAbstractStorageResource::cap::WriteFile;
            reply.storage.isUsedForWriting = storage->isUsedForWriting();
        }
    }
        
    result.setReply(reply);
    return CODE_OK;
}

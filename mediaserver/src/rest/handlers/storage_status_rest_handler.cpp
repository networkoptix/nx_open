#include "storage_status_rest_handler.h"

#include <QtCore/QFileInfo>

#include "utils/network/tcp_connection_priv.h"

#include "core/resource_management/resource_pool.h"
#include <core/resource/storage_resource.h>

#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "recorder/storage_manager.h"
#include "api/model/storage_status_reply.h"
#include "media_server/settings.h"


int QnStorageStatusRestHandler::executeGet(const QString &, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString storageUrl;
    if(!requireParameter(params, lit("path"), result, &storageUrl))
        return CODE_INVALID_PARAMETER;

    QnStorageResourcePtr storage = qnStorageMan->getStorageByUrl(storageUrl);
    bool exists = storage;
    if (!storage) {
        storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(storageUrl, false));
        if(storage) {
            storage->setUrl(storageUrl);
            storage->setSpaceLimit(nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE);
        }
    }
    
    QnStorageStatusReply reply;
    reply.pluginExists = storage;
    reply.storage.storageId = exists ? storage->getId() : QUuid();
    reply.storage.url = storageUrl;
    reply.storage.freeSpace = storage ? storage->getFreeSpace() : -1;
    reply.storage.reservedSpace = storage ? storage->getSpaceLimit() : -1;
    reply.storage.totalSpace = storage ? storage->getTotalSpace() : -1;
    reply.storage.isExternal = storageUrl.trimmed().startsWith(lit("\\\\")) || QUrl(storageUrl).path().mid(1).startsWith(lit("\\\\"));  // TODO: #Elric not consistent with space_handler
#ifdef WIN32
    if (!reply.storage.isExternal) {
        reply.storage.isWritable = false;
        reply.storage.isUsedForWriting = false;
    }
    else 
#endif
    {
        reply.storage.isWritable = storage ? storage->isStorageAvailableForWriting() : false;
        reply.storage.isUsedForWriting = exists ? storage->isUsedForWriting() : false;
    }
        
    // TODO: #Elric remove once UnknownSize is dropped.
    if(reply.storage.totalSpace == QnStorageResource::UnknownSize)
        reply.storage.totalSpace = -1;
    if(reply.storage.freeSpace == QnStorageResource::UnknownSize)
        reply.storage.freeSpace = -1;

    result.setReply(reply);
    return CODE_OK;
}

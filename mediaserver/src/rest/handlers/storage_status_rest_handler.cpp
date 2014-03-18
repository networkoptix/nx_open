#include "storage_status_rest_handler.h"

#include <QtCore/QFileInfo>

#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "recorder/storage_manager.h"
#include "api/model/storage_status_reply.h"

int QnStorageStatusRestHandler::executeGet(const QString &, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString storageUrl;
    if(!requireParameter(params, lit("path"), result, &storageUrl))
        return CODE_INVALID_PARAMETER;

    QnStorageResourcePtr storage = qnStorageMan->getStorageByUrl(storageUrl);
    bool exists = storage;
    if (!storage) {
        storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(storageUrl, false));
        if(storage)
            storage->setUrl(storageUrl);
    }
    
    QnStorageStatusReply reply;
    reply.pluginExists = storage;
    reply.storage.storageId = exists ? storage->getId().toInt() : -1;
    reply.storage.path = storageUrl;
    reply.storage.freeSpace = storage ? storage->getFreeSpace() : -1;
    reply.storage.reservedSpace = storage ? storage->getSpaceLimit() : -1;
    reply.storage.totalSpace = storage ? storage->getTotalSpace() : -1;
    reply.storage.isExternal = storageUrl.contains(QLatin1String("://")) || storageUrl.trimmed().startsWith(QLatin1String("\\\\")); // TODO: #Elric not consistent with space_handler
    reply.storage.isWritable = storage ? storage->isStorageAvailableForWriting() : false;
    reply.storage.isUsedForWriting = exists ? storage->isUsedForWriting() : false;
        
    // TODO: #Elric remove once UnknownSize is dropped.
    if(reply.storage.totalSpace == QnStorageResource::UnknownSize)
        reply.storage.totalSpace = -1;
    if(reply.storage.freeSpace == QnStorageResource::UnknownSize)
        reply.storage.freeSpace = -1;

    result.setReply(reply);
    return CODE_OK;
}

QString QnStorageStatusRestHandler::description() const
{
    return 
        "Returns 'OK' if specified folder may be used for writing on mediaServer. Otherwise returns 'FAIL' \n"
        "<BR>Param <b>path</b> - Folder."
        "<BR><b>Return</b> JSON with path validity.";
}

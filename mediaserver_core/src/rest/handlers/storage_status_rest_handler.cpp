#include "storage_status_rest_handler.h"

#include <QtCore/QFileInfo>

#include "network/tcp_connection_priv.h"

#include "core/resource_management/resource_pool.h"
#include <core/resource/storage_resource.h>
#include <core/resource/storage_plugin_factory.h>
#include <plugins/storage/file_storage/file_storage_resource.h>

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
        qint64 spaceLimit = QnFileStorageResource::isLocal(storageUrl) ?
                            nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE :
                            QnFileStorageResource::kNasStorageLimit;

        storage->setUrl(storageUrl);
        storage->setSpaceLimit(spaceLimit);
        if (!storage || !storage->initOrUpdate())
            return CODE_INVALID_PARAMETER;
    }

    NX_ASSERT(storage, Q_FUNC_INFO, "Storage must exist here");
    bool exists = !storage.isNull();

    QnStorageStatusReply reply;
    reply.pluginExists = exists;
    reply.storage.url  = storageUrl;

    if (storage) {
        reply.storage = QnStorageSpaceData(storage, false);
#ifdef WIN32
        if (!reply.storage.isExternal) {
            /* Do not allow to create several local storages on one hard drive. */
            reply.storage.isWritable = false;
            reply.storage.isUsedForWriting = false;
        }
#endif
    }

    result.setReply(reply);
    return CODE_OK;
}

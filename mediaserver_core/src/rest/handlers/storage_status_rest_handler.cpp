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
#include <rest/server/rest_connection_processor.h>


namespace aux {
QnStorageStatusReply createReply(const QnStorageResourcePtr& storage)
{
    QnStorageStatusReply reply;
    reply.status = storage->initOrUpdate();
    reply.storage.url  = storage->getUrl();
    reply.storage = QnStorageSpaceData(storage, false);

    QnFileStorageResourcePtr fileStorage = storage.dynamicCast<QnFileStorageResource>();
    if (fileStorage)
        reply.storage.reservedSpace = fileStorage->calcInitialSpaceLimit();
    else
        reply.storage.reservedSpace = QnStorageResource::kThirdPartyStorageLimit;

#if defined (Q_OS_WIN)
    if (!reply.storage.isExternal)
    {
        /* Do not allow to create several local storages on one hard drive. */
        reply.storage.isWritable = false;
        reply.storage.isUsedForWriting = false;
    }
#endif

    return reply;
}

QnStorageResourcePtr getOrCreateStorage(QnCommonModule* commonModule, const QString& storageUrl)
{
    QnStorageResourcePtr storage = qnNormalStorageMan->getStorageByUrlExact(storageUrl);
    if (!storage)
        storage = qnBackupStorageMan->getStorageByUrlExact(storageUrl);

    if (!storage)
        storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(commonModule, storageUrl, false));

    if (storage)
        storage->setUrl(storageUrl);

    return storage;
}
}

int QnStorageStatusRestHandler::executeGet(
    const QString &,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    QString storageUrl;

    if(!requireParameter(params, lit("path"), result, &storageUrl))
        return nx::network::http::StatusCode::invalidParameter;

    auto storage = aux::getOrCreateStorage(owner->commonModule(), storageUrl);
    if (!storage)
        return nx::network::http::StatusCode::invalidParameter;

    result.setReply(aux::createReply(storage));
    return nx::network::http::StatusCode::ok;
}

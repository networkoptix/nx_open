#include "rebuild_archive_rest_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include <media_server/serverutil.h>
#include "qcoreapplication.h"
#include <media_server/settings.h>
#include "recorder/device_file_catalog.h"
#include "recorder/storage_manager.h"
#include "core/resource/storage_resource.h"

int QnRebuildArchiveRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    QString method = params.value("action");
    bool useMainPool = !params.contains("mainPool") || params.value("mainPool").toInt() != 0;
    auto storagePool = useMainPool ? QnStorageManager::normalInstance() : QnStorageManager::backupInstance();

    auto reply = storagePool->rebuildInfo();
    if (method == "start")
        reply = storagePool->rebuildCatalogAsync();
    else if (method == "stop")
        storagePool->cancelRebuildCatalogAsync();

    result.setReply(reply);
    return CODE_OK;
}

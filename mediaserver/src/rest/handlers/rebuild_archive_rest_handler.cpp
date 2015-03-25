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
    QString method = params.value("action");
    auto reply = QnStorageManager::instance()->rebuildInfo();
    if (method == "start")
        reply = QnStorageManager::instance()->rebuildCatalogAsync();
    else if (method == "stop")
        QnStorageManager::instance()->cancelRebuildCatalogAsync();

    result.setReply(reply);
    return CODE_OK;
}

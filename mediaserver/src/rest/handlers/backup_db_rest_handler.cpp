#include "backup_db_rest_handler.h"

#include "utils/network/tcp_connection_priv.h"
#include "media_server/serverutil.h"

int QnBackupDbRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)
    Q_UNUSED(result)
    Q_UNUSED(params)

    if (!backupDatabase())
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

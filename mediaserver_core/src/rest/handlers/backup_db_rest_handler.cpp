#include "backup_db_rest_handler.h"

#include "network/tcp_connection_priv.h"
#include "media_server/serverutil.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

int QnBackupDbRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)
    Q_UNUSED(result)
    Q_UNUSED(params)

    if (!backupDatabase(owner->commonModule()->ec2Connection()))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

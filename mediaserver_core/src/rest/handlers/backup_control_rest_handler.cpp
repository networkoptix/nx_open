#include "backup_control_rest_handler.h"
#include "network/tcp_connection_priv.h"
#include "utils/common/util.h"
#include "api/model/backup_status_reply.h"
#include <recorder/schedule_sync.h>
#include "recorder/storage_manager.h"

int QnBackupControlRestHandler::executeGet(const QString& /*path*/, const QnRequestParams& params,
    QnJsonRestResult& result, const QnRestConnectionProcessor*)
{
    QString method = params.value("action");
    QnBackupStatusData reply;

    if (method == "start")
        qnBackupStorageMan->scheduleSync()->forceStart();
    else if (method == "stop")
        qnBackupStorageMan->scheduleSync()->interrupt();

    reply = qnBackupStorageMan->scheduleSync()->getStatus();

    result.setReply(reply);
    return CODE_OK;
}

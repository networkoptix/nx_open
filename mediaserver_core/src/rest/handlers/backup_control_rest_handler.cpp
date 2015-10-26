#include "backup_control_rest_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/util.h"
#include "api/model/backup_status_reply.h"
#include <recorder/schedule_sync.h>

int QnBackupControlRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    
    QString method = params.value("action");
    QnBackupStatusData reply;
    
    if (method == "start")
        qnScheduleSync->forceStart();
    else if (method == "stop")
        qnScheduleSync->interrupt();

    reply = qnScheduleSync->getStatus();

    result.setReply(reply);
    return CODE_OK;
}

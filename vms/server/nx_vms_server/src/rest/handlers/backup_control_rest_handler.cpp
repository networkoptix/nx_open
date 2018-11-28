#include "backup_control_rest_handler.h"
#include "network/tcp_connection_priv.h"
#include "utils/common/util.h"
#include "api/model/backup_status_reply.h"
#include <recorder/schedule_sync.h>
#include "recorder/storage_manager.h"

QnBackupControlRestHandler::QnBackupControlRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnBackupControlRestHandler::executeGet(const QString& /*path*/, const QnRequestParams& params,
    QnJsonRestResult& result, const QnRestConnectionProcessor*)
{
    QString method = params.value("action");
    QnBackupStatusData reply;

    if (method == "start")
        serverModule()->backupStorageManager()->scheduleSync()->forceStart();
    else if (method == "stop")
        serverModule()->backupStorageManager()->scheduleSync()->interrupt();

    reply = serverModule()->backupStorageManager()->scheduleSync()->getStatus();

    result.setReply(reply);
    return nx::network::http::StatusCode::ok;
}

#include "backup_control_rest_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/util.h"
#include "api/model/backup_status_reply.h"

int QnBackupControlRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    
    QString method = params.value("action");
    QnBackupStatusData reply;
    
    /*
    reply = QnStorageManager::backupInstance()->backupStatus();
    if (method == "start")
        reply = QnStorageManager::backupInstance()->startBackupData();
    else if (method == "stop")
        QnStorageManager::backupInstance()->stopBackupData();
    */

    result.setReply(reply);
    return CODE_OK;
}

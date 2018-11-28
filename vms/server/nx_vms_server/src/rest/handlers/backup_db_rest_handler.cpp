#include "backup_db_rest_handler.h"

#include "network/tcp_connection_priv.h"
#include "media_server/serverutil.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

QnBackupDbRestHandler::QnBackupDbRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnBackupDbRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult& /*result*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    nx::vms::server::Utils utils(serverModule());
    if (!utils.backupDatabase())
        return nx::network::http::StatusCode::internalServerError;

    return nx::network::http::StatusCode::ok;
}

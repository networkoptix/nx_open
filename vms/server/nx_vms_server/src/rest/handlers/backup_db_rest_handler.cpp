#include "backup_db_rest_handler.h"

#include "network/tcp_connection_priv.h"
#include "media_server/serverutil.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx/network/rest/nx_network_rest_ini.h>

using namespace nx::network;

namespace nx::vms::server {

BackupDbRestHandler::BackupDbRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

rest::Response BackupDbRestHandler::executeGet(const rest::Request& request)
{
    if (!rest::ini().allowGetModifications)
        return rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

rest::Response BackupDbRestHandler::executePost(const rest::Request& /*request*/)
{
    nx::vms::server::Utils utils(serverModule());
    if (!utils.backupDatabase())
        return rest::Response::error(rest::Result::InternalServerError);

    return rest::Response::result(rest::JsonResult());
}

} // namespace nx::vms::server

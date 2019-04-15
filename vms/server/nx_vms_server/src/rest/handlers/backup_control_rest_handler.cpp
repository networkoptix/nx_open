#include "backup_control_rest_handler.h"
#include "network/tcp_connection_priv.h"
#include "utils/common/util.h"
#include "api/model/backup_status_reply.h"
#include <recorder/schedule_sync.h>
#include "recorder/storage_manager.h"
#include <nx/network/rest/nx_network_rest_ini.h>

static const QString kAction("action");

using namespace nx::network;

namespace nx::vms::server {

BackupControlRestHandler::BackupControlRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

nx::network::rest::Response BackupControlRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    // TODO: This branch should be deprecated.
    if (const auto action = request.param(kAction))
    {
        if (!nx::network::rest::ini().allowGetModifications)
            throw nx::network::rest::Exception(nx::network::rest::Result::Forbidden);

        execute(*action);
    }

    return rest::Response::reply(
        serverModule()->backupStorageManager()->scheduleSync()->getStatus());
}

nx::network::rest::Response BackupControlRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    if (const auto action = request.param(kAction))
        execute(*action);

    return nx::network::rest::Response::reply(
        serverModule()->backupStorageManager()->scheduleSync()->getStatus());
}

int BackupControlRestHandler::execute(const QString& action)
{
    if (action == "start")
        return serverModule()->backupStorageManager()->scheduleSync()->forceStart();

    if (action == "stop")
        return serverModule()->backupStorageManager()->scheduleSync()->interrupt();

    throw nx::network::rest::Exception(nx::network::rest::Result::InvalidParameter, kAction, action);
}

} // namespace nx::vms::server

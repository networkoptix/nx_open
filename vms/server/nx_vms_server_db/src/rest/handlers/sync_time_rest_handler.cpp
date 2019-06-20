#include "sync_time_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx_ec/ec_api.h>

namespace rest::handlers {

nx::network::rest::Response SyncTimeRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    auto timeSyncManager = request.owner->commonModule()->ec2Connection()->timeSyncManager();
    return nx::network::rest::Response::reply(execute(timeSyncManager));
}

SyncTimeData SyncTimeRestHandler::execute(
    nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager)
{
    SyncTimeData reply;
    reply.utcTimeMs = timeSyncManager->getSyncTime(&reply.isTakenFromInternet).count();
    return reply;
}

} // namespace rest::handlers

#include "sync_time_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx_ec/ec_api.h>

namespace rest {
namespace handlers {

int SyncTimeRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    auto timeSyncManager = owner->commonModule()->ec2Connection()->timeSyncManager();
    result.setReply(execute(timeSyncManager));
    return nx::network::http::StatusCode::ok;
}

SyncTimeData SyncTimeRestHandler::execute(nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager)
{
    SyncTimeData reply;
    reply.utcTimeMs = timeSyncManager->getSyncTime(&reply.isTakenFromInternet).count();
    return reply;
}

} // namespace handlers
} // namespace rest

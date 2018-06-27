#include "sync_time_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx_ec/ec_api.h>
#include <nx/time_sync/time_sync_manager.h>

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

SyncTimeData SyncTimeRestHandler::execute(nx::time_sync::TimeSyncManager* timeSyncManager)
{
    SyncTimeData reply;
    reply.isTakenFromInternet = timeSyncManager->isTimeTakenFromInternet();
    reply.utcTimeMs = timeSyncManager->getSyncTime().count();
    return reply;
}

} // namespace handlers
} // namespace rest

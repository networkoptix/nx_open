#include "time_rest_handler.h"

#include <api/model/time_reply.h>

#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/utils/time.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx/network/app_info.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

int QnTimeRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnTimeReply reply;
    reply.timeZoneOffset = currentTimeZone() * 1000LL;

    // TODO: Remove parameter "local", making its behavior the default. Check nxtool.
    auto commonModule = owner->commonModule();
    if (params.contains("local"))
        reply.utcTime =  QDateTime::currentDateTime().toMSecsSinceEpoch();
    else
        reply.utcTime = commonModule->ec2Connection()->timeSyncManager()->getSyncTime().count();

    reply.timezoneId = nx::utils::getCurrentTimeZoneId();

    reply.realm = nx::network::AppInfo::realm();

    result.setReply(reply);
    return nx::network::http::StatusCode::ok;
}

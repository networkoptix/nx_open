#include "time_rest_handler.h"

#include <api/model/time_reply.h>

#include <network/authenticate_helper.h>
#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/utils/time.h>

int QnTimeRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* /*owner*/)
{
    QnTimeReply reply;
    reply.timeZoneOffset = currentTimeZone() * 1000LL;

    // TODO: Remove parameter "local", making its behavior the default. Check nxtool.
    if (params.contains("local"))
        reply.utcTime =  QDateTime::currentDateTime().toMSecsSinceEpoch();
    else
        reply.utcTime = qnSyncTime->currentMSecsSinceEpoch();

    reply.timezoneId = nx::utils::getCurrentTimeZoneId();

    reply.realm = nx::network::AppInfo::realm();

    result.setReply(reply);
    return CODE_OK;
}

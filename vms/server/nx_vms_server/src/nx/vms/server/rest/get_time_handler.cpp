#include "get_time_handler.h"

#include <nx/vms/api/data/time_reply.h>

#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/utils/time.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx/network/app_info.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

namespace nx::vms::server::rest {

using namespace std::chrono;

int GetTimeHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    nx::vms::api::TimeReply reply;

    auto commonModule = owner->commonModule();
    reply.osTime = milliseconds(QDateTime::currentDateTime().toMSecsSinceEpoch());
    reply.vmsTime = commonModule->ec2Connection()->timeSyncManager()->getSyncTime();
    reply.timeZoneOffset = milliseconds(currentTimeZone() * 1000LL);

    reply.timeZoneId = nx::utils::getCurrentTimeZoneId();
    reply.realm = nx::network::AppInfo::realm();

    result.setReply(reply);
    return nx::network::http::StatusCode::ok;
}

} // namespace nx::vms::server::rest
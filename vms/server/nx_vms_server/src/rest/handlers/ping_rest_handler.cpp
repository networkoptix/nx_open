#include "ping_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <rest/helper/ping_rest_helper.h>

int QnPingRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    result.setReply(rest::helper::PingRestHelper::data(owner->commonModule()));
    return nx::network::http::StatusCode::ok;
}

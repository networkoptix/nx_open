
#include "ping_rest_handler.h"

#include <api/model/ping_reply.h>
#include <common/common_module.h>
#include <media_server/serverutil.h>
#include <utils/network/http/httptypes.h>


int QnPingRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    QnPingReply reply;
    reply.moduleGuid = qnCommon->moduleGUID();

    result.setReply( reply );
    return nx_http::StatusCode::ok;
}


#include "ping_rest_handler.h"

#include <api/model/ping_reply.h>
#include <common/common_module.h>
#include <media_server/serverutil.h>
#include <nx/network/http/httptypes.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

int QnPingRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();

    QnPingReply reply;
    reply.moduleGuid = qnCommon->moduleGUID();
    reply.localSystemId = qnGlobalSettings->localSystemId();
    reply.sysIdTime = qnCommon->systemIdentityTime();
    reply.tranLogTime = ec2Connection->getTransactionLogTime();

    result.setReply( reply );
    return nx_http::StatusCode::ok;
}

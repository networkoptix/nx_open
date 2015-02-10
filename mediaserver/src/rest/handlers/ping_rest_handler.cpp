
#include "ping_rest_handler.h"

#include <api/model/ping_reply.h>
#include <common/common_module.h>
#include <media_server/serverutil.h>
#include <utils/network/http/httptypes.h>

#include <api/app_server_connection.h>

int QnPingRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    QnPingReply reply;
    reply.moduleGuid = qnCommon->moduleGUID();
    reply.systemName = qnCommon->localSystemName();
    reply.sysIdTime = qnCommon->systemIdentityTime();
    reply.tranLogTime = ec2Connection->getTransactionLogTime();
    
    result.setReply( reply );
    return nx_http::StatusCode::ok;
}

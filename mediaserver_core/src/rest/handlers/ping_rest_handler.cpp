
#include "ping_rest_handler.h"

#include <api/model/ping_reply.h>
#include <common/common_module.h>
#include <media_server/serverutil.h>
#include <nx/network/http/http_types.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <rest/server/rest_connection_processor.h>

int QnPingRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    ec2::AbstractECConnectionPtr ec2Connection = owner->commonModule()->ec2Connection();

    QnPingReply reply;
    reply.moduleGuid = owner->commonModule()->moduleGUID();
    reply.localSystemId = owner->commonModule()->globalSettings()->localSystemId();
    reply.sysIdTime = owner->commonModule()->systemIdentityTime();
    reply.tranLogTime = ec2Connection->getTransactionLogTime();

    result.setReply( reply );
    return nx_http::StatusCode::ok;
}

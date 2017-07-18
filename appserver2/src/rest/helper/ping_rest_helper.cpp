#include "ping_rest_helper.h"

#include <common/common_module.h>
#include <api/app_server_connection.h>
#include <api/global_settings.h>

namespace rest {
namespace helper {

QnPingReply PingRestHelper::data(const QnCommonModule* commonModule)
{
    ec2::AbstractECConnectionPtr ec2Connection = commonModule->ec2Connection();

    QnPingReply reply;
    reply.moduleGuid = commonModule->moduleGUID();
    reply.localSystemId = commonModule->globalSettings()->localSystemId();
    reply.sysIdTime = commonModule->systemIdentityTime();
    reply.tranLogTime = ec2Connection->getTransactionLogTime();
    return reply;
}

} // namespace helper
} // namespace rest

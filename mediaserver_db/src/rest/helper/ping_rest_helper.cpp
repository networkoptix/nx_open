#include "ping_rest_helper.h"

#include <common/common_module.h>
#include <api/global_settings.h>

#include <nx_ec/ec_api.h>

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

#pragma once

#include <nx/utils/thread/mutex.h>

#include <utils/common/request_param.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/schedule_task.h>

#include <licensing/license.h>
#include <nx_ec/ec_api.h>

#include <api/model/servers_reply.h>
#include <api/model/connection_info.h>

class QnAppServerConnectionFactory;
class QnApiSerializer;

class QN_EXPORT QnAppServerConnectionFactory
{
public:
    static void setEc2Connection(const ec2::AbstractECConnectionPtr &connection );
    static ec2::AbstractECConnectionPtr ec2Connection(); // renamed to show compile errors. use commonModule->ec2Connection() instead

};

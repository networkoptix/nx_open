#include "app_server_connection.h"
#include "nx/utils/log/log.h"

static ec2::AbstractECConnectionPtr currentlyUsedEc2Connection;
static std::mutex ec2ConnectionLock;

void QnAppServerConnectionFactory::setEc2Connection(const ec2::AbstractECConnectionPtr &ec2Connection )
{
    if (ec2Connection == nullptr)
        NX_VERBOSE(NX_SCOPE_TAG, "cleaning ec2Connection");
    else
        NX_VERBOSE(NX_SCOPE_TAG, "setting connection");
    std::lock_guard<std::mutex> guard(ec2ConnectionLock);
    currentlyUsedEc2Connection = ec2Connection;
}

ec2::AbstractECConnectionPtr QnAppServerConnectionFactory::ec2Connection()
{
    std::lock_guard<std::mutex> guard(ec2ConnectionLock);
    return currentlyUsedEc2Connection;
}

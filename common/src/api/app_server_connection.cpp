#include "app_server_connection.h"

static ec2::AbstractECConnectionPtr currentlyUsedEc2Connection;

void QnAppServerConnectionFactory::setEc2Connection(const ec2::AbstractECConnectionPtr &ec2Connection )
{
    currentlyUsedEc2Connection = ec2Connection;
}

ec2::AbstractECConnectionPtr QnAppServerConnectionFactory::ec2Connection()
{
    return currentlyUsedEc2Connection;
}

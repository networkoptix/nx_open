#pragma once

#include <nx_ec/ec_api_fwd.h>

class QN_EXPORT QnAppServerConnectionFactory
{
public:
    static void setEc2Connection(const ec2::AbstractECConnectionPtr &connection );

    // Renamed to show compile errors. use commonModule->ec2Connection() instead.
    static ec2::AbstractECConnectionPtr ec2Connection();
};

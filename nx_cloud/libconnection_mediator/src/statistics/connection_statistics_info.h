#pragma once

#include <chrono>

#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/socket_common.h>

namespace nx {
namespace hpm {
namespace stats {

struct ConnectSession
{
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    api::NatTraversalResultCode resultCode;
    nx::String sessionId;
    SocketAddress originatingHostEndpoint;
    nx::String originatingHostName;
    SocketAddress destinationHostEndpoint;
    nx::String destinationHostName;

    ConnectSession():
        resultCode(api::NatTraversalResultCode::ok)
    {
    }
};

} // namespace stats
} // namespace hpm
} // namespace nx

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
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
    nx::String originatingHostEndpoint;
    nx::String originatingHostName;
    nx::String destinationHostEndpoint;
    nx::String destinationHostName;

    ConnectSession();

    bool operator==(const ConnectSession& rhs) const;
};

#define ConnectSession_Fields (startTime)(endTime)(resultCode)(sessionId) \
    (originatingHostEndpoint)(originatingHostName)(destinationHostEndpoint)(destinationHostName)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ConnectSession),
    (sql_record))

} // namespace stats
} // namespace hpm
} // namespace nx
